/**
 * GitHub Action script for managing issue backlog.
 * 
 * Behavior:
 * - Pull Requests are skipped (only opened issues are processed)
 * - Skips issues with labels defined in 'exemptLabels'
 * - Closes issues with labels defined in 'closeLabels' or without assignees, 
 *   with a standard closure comment.
 * - Sends a Friendly Reminder comment to assigned issues without 
 *   exempt labels that have been inactive for 90+ days.
 * - Avoids sending duplicate Friendly Reminder comments if one was 
 *   posted within the last 7 days.
 * - Marks issues labeled 'Type: Question' by adding the 'Move to Discussion' label.
 *   (Actual migration to Discussions must be handled manually due to API limitations.)
 */

const dedent = (strings, ...values) => {
    const raw = typeof strings === 'string' ? [strings] : strings.raw;
    let result = '';
    raw.forEach((str, i) => {
        result += str + (values[i] || '');
    });
    const lines = result.split('\n');
    if (!lines.some(l => l.trim())) return '';
    const minIndent = Math.min(...lines.filter(l => l.trim()).map(l => l.match(/^\s*/)[0].length));
    return lines.map(l => l.slice(minIndent)).join('\n').trim();
};


async function addMoveToDiscussionLabel(github, owner, repo, issue, isDryRun) {
    const targetLabel = "Move to Discussion";

    const hasLabel = issue.labels.some(
        l => l.name.toLowerCase() === targetLabel.toLowerCase()
    );

    if (hasLabel) return false;

    if (isDryRun) {
        console.log(`[DRY-RUN] Would add '${targetLabel}' to issue #${issue.number}`);
        return true;
    }

    try {
        await github.rest.issues.addLabels({
            owner,
            repo,
            issue_number: issue.number,
            labels: [targetLabel],
        });
        console.log(`Adding label to #${issue.number} (Move to discussion)`);
        return true;

    } catch (err) {
        console.error(`Failed to add label to #${issue.number}`, err);
        return false;
    }
}


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


async function hasRecentFriendlyReminder(github, owner, repo, issueNumber, maxAgeMs) {
    let page = 1;

    while (true) {
        const { data } = await github.rest.issues.listComments({
            owner,
            repo,
            issue_number: issueNumber,
            per_page: 100,
            page,
        });
        if (!data || data.length === 0) break;
        
        for (const c of data) {
            if (c.user?.login === 'github-actions[bot]' && 
                c.body.includes('<!-- backlog-bot:friendly-reminder -->')) 
            {
                const created = new Date(c.created_at).getTime();
                if (Date.now() - created < maxAgeMs) {
                    return true;
                }
            }
        }
        
        if (data.length < 100) break;
        page++;
    }
    return false;
}


module.exports = async ({ github, context, dryRun }) => {
    const now = new Date();
    const thresholdDays = 90;
    const exemptLabels = [
        'Status: Community help needed',
        'Status: Needs investigation',
        'Move to Discussion',
        'Status: Blocked upstream 🛑', 
        'Status: Blocked by ESP-IDF 🛑'
    ];
    const closeLabels = ['Status: Awaiting Response'];
    const questionLabel = 'Type: Question';
    const { owner, repo } = context.repo;
    const sevenDaysMs = 7 * 24 * 60 * 60 * 1000;

    const isDryRun = dryRun === "1";
    if (isDryRun) {
        console.log("DRY-RUN mode enabled — no changes will be made.");
    }

    let totalClosed = 0;
    let totalReminders = 0;
    let totalSkipped = 0;
    let totalMarkedToMigrate = 0;

    let issues = [];

    try {
        issues = await fetchAllOpenIssues(github, owner, repo);
    } catch (err) {
        console.error('Failed to fetch issues:', err);
        return;
    }

    for (const issue of issues) {
        const isAssigned = issue.assignees && issue.assignees.length > 0;
        const lastUpdate = new Date(issue.updated_at);
        const oneDayMs = 1000 * 60 * 60 * 24;
        const daysSinceUpdate = Math.floor((now - lastUpdate) / oneDayMs);

        if (issue.labels.some(label => exemptLabels.includes(label.name))) {
            console.log(`Skipping #${issue.number} (exempt label)`);
            totalSkipped++;
            continue;
        }

        if (issue.labels.some(label => label.name === questionLabel)) {
            const marked = await addMoveToDiscussionLabel(github, owner, repo, issue, isDryRun);
            if (marked) totalMarkedToMigrate++;
            continue; // Do not apply reminder logic
        }

        if (daysSinceUpdate < thresholdDays) {
            console.log(`Skipping #${issue.number} (recent activity)`);
            totalSkipped++;
            continue;
        }

        if (issue.labels.some(label => closeLabels.includes(label.name)) || !isAssigned) {

            if (isDryRun) {
                console.log(`[DRY-RUN] Would close issue #${issue.number}`);
                totalClosed++;
                continue;
            }

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
                console.log(`Closing #${issue.number} (inactivity)`);
                totalClosed++;
            } catch (err) {
                console.error(`Error closing issue #${issue.number}:`, err);
            }
            continue;
        }

        if (isAssigned) {

            if (await hasRecentFriendlyReminder(github, owner, repo, issue.number, sevenDaysMs)) {
                console.log(`Skipping #${issue.number} (recent reminder)`);
                totalSkipped++;
                continue;
            }

            const assignees = issue.assignees.map(u => `@${u.login}`).join(', ');
            const comment = dedent`
                <!-- backlog-bot:friendly-reminder -->
                ⏰ Friendly Reminder

                Hi ${assignees}!

                This issue has had no activity for ${daysSinceUpdate} days. If it's still relevant:
                - Please provide a status update
                - Add any blocking details and labels
                - Or label it 'Status: Awaiting Response' if you're waiting on the user's response

                This is just a reminder; the issue remains open for now.`;

            if (isDryRun) {
                console.log(`[DRY-RUN] Would post reminder on #${issue.number}`);
                totalReminders++;
                continue;
            }
            
            try {
                await github.rest.issues.createComment({
                    owner,
                    repo,
                    issue_number: issue.number,
                    body: comment,
                });
                console.log(`Sending reminder to #${issue.number}`);
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
        Total marked to migrate to discussions: ${totalMarkedToMigrate}
        Total skipped: ${totalSkipped}`);
};
