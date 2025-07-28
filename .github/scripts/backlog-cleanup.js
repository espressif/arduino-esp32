/**
 * GitHub Action script for managing issue backlog.
 * 
 * Behavior:
 * - Pull Requests are skipped (only opened issues are processed)
 * - Skips issues with 'to-be-discussed' label.
 * - Closes issues with label 'awaiting-response' or without assignees, 
 *   with a standard closure comment.
 * - Sends a Friendly Reminder comment to assigned issues without 
 *   exempt labels that have been inactive for 90+ days.
 * - Avoids sending duplicate Friendly Reminder comments if one was 
 *   posted within the last 7 days.
 * - Moves issues labeled 'questions' to GitHub Discussions
 */

const dedent = (strings, ...values) => {
    const raw = typeof strings === 'string' ? [strings] : strings.raw;
    let result = '';
    raw.forEach((str, i) => {
        result += str + (values[i] || '');
    });
    const lines = result.split('\n');
    const minIndent = Math.min(...lines.filter(l => l.trim()).map(l => l.match(/^\s*/)[0].length));
    return lines.map(l => l.slice(minIndent)).join('\n').trim();
};

async function fetchAllOpenIssues(github, owner, repo) {
    const issues = [];
    let page = 1;

    while (true) {
        try {
            const response = await github.rest.issues.listForRepo({
                owner,
                repo,
                state: 'open',
                per_page: 100,
                page,
            });
            const data = response.data || [];
            if (data.length === 0) break;
            const onlyIssues = data.filter(issue => !issue.pull_request);
            issues.push(...onlyIssues);
            if (data.length < 100) break;
            page++;
        } catch (err) {
            console.error('Error fetching issues:', err);
            break;
        }
    }
    return issues;
}


async function migrateToDiscussion(github, owner, repo, issue, categories) {
    const discussionCategory = 'Q&A';
    try {
        const category = categories.find(cat =>
            cat.name.toLowerCase() === discussionCategory.toLowerCase()
        );
        if (!category) {
            throw new Error(`Discussion category '${discussionCategory}' not found.`);
        }
        const { data: discussion } = await github.rest.discussions.create({
            owner,
            repo,
            title: issue.title,
            body: `Originally created by @${issue.user.login} in #${issue.number}\n\n---\n\n${issue.body || ''}`,
            category_id: category.id,
        });
        await github.rest.issues.createComment({
            owner,
            repo,
            issue_number: issue.number,
            body: `💬 This issue was moved to [Discussions](${discussion.html_url}) for better visibility.`,
        });
        await github.rest.issues.update({
            owner,
            repo,
            issue_number: issue.number,
            state: 'closed',
        });
        return discussion.html_url;
    } catch (err) {
        console.error(`Error migrating issue #${issue.number} to discussion:`, err);
        return null;
    }
}


const shouldSendReminder = (issue, exemptLabels, closeLabels) => {
    const hasExempt = issue.labels.some(l => exemptLabels.includes(l.name));
    const hasClose = issue.labels.some(l => closeLabels.includes(l.name));
    return issue.assignees.length > 0 && !hasExempt && !hasClose;
};


module.exports = async ({ github, context }) => {
    const now = new Date();
    const thresholdDays = 90;
    const exemptLabels = ['Status: Community help needed', 'Status: Needs investigation'];
    const closeLabels = ['Status: Awaiting Response'];
    const discussionLabel = 'Type: Question';
    const { owner, repo } = context.repo;

    let totalClosed = 0;
    let totalReminders = 0;
    let totalSkipped = 0;
    let totalMigrated = 0;
    let issues = [];
    let categories = [];

    try {
        issues = await fetchAllOpenIssues(github, owner, repo);
    } catch (err) {
        console.error('Failed to fetch issues:', err);
        return;
    }

    try {
        const { data } = await github.rest.discussions.listCategories({ owner, repo });
        categories = data;
    } catch (err) {
        console.error('Error fetching discussion categories:', err);
    }

    for (const issue of issues) {
        const isAssigned = issue.assignees && issue.assignees.length > 0;
        const lastUpdate = new Date(issue.updated_at);
        const daysSinceUpdate = Math.floor((now - lastUpdate) / (1000 * 60 * 60 * 24));

        if (issue.labels.some(label => label.name === discussionLabel)) {
            const migrated = await migrateToDiscussion(github, owner, repo, issue, categories);
            if (migrated) totalMigrated++;
            continue;
        }

        if (daysSinceUpdate < thresholdDays) {
            totalSkipped++;
            continue;
        }

        if (issue.labels.some(label => exemptLabels.includes(label.name))) {
            totalSkipped++;
            continue;
        }

        if (issue.labels.some(label => closeLabels.includes(label.name)) || !isAssigned) {
            try {
                await github.rest.issues.createComment({
                    owner,
                    repo,
                    issue_number: issue.number,
                    body: '⚠️ This issue was closed automatically due to inactivity. Please reopen or open a new one if still relevant.',
                });
                await github.rest.issues.update({
                    owner,
                    repo,
                    issue_number: issue.number,
                    state: 'closed',
                });
                totalClosed++;
            } catch (err) {
                console.error(`Error closing issue #${issue.number}:`, err);
            }
            continue;
        }

        let comments = [];
        try {
            const { data } = await github.rest.issues.listComments({
                owner,
                repo,
                issue_number: issue.number,
                per_page: 50,
            });
            comments = data;
        } catch (err) {
            console.error(`Error fetching comments for issue #${issue.number}:`, err);
        }

        const recentFriendlyReminder = comments.find(comment =>
            comment.user.login === 'github-actions[bot]' &&
            comment.body.includes('⏰ Friendly Reminder')
        );
        if (recentFriendlyReminder) {
            totalSkipped++;
            continue;
        }

        if (shouldSendReminder(issue, exemptLabels, closeLabels)) {
            const assignees = issue.assignees.map(u => `@${u.login}`).join(', ');
            const comment = dedent`
                ⏰ Friendly Reminder

                Hi ${assignees}!

                This issue has had no activity for ${daysSinceUpdate} days. If it's still relevant:
                - Please provide a status update
                - Add any blocking details
                - Or label it 'awaiting-response' if you're waiting on something

                This is just a reminder; the issue remains open for now.`;
            try {
                await github.rest.issues.createComment({
                    owner,
                    repo,
                    issue_number: issue.number,
                    body: comment,
                });
                totalReminders++;
            } catch (err) {
                console.error(`Error sending reminder for issue #${issue.number}:`, err);
            }
        }
    }

    console.log(dedent`
        === Backlog cleanup summary ===
        Total issues processed: ${issues.length}
        Total issues closed: ${totalClosed}
        Total reminders sent: ${totalReminders}
        Total migrated to discussions: ${totalMigrated}
        Total skipped: ${totalSkipped}`);
};
