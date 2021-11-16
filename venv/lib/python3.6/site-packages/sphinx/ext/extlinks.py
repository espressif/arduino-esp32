"""
    sphinx.ext.extlinks
    ~~~~~~~~~~~~~~~~~~~

    Extension to save typing and prevent hard-coding of base URLs in the reST
    files.

    This adds a new config value called ``extlinks`` that is created like this::

       extlinks = {'exmpl': ('https://example.invalid/%s.html', caption), ...}

    Now you can use e.g. :exmpl:`foo` in your documents.  This will create a
    link to ``https://example.invalid/foo.html``.  The link caption depends on
    the *caption* value given:

    - If it is ``None``, the caption will be the full URL.
    - If it is a string, it must contain ``%s`` exactly once.  In this case the
      caption will be *caption* with the role content substituted for ``%s``.

    You can also give an explicit caption, e.g. :exmpl:`Foo <foo>`.

    Both, the url string and the caption string must escape ``%`` as ``%%``.

    :copyright: Copyright 2007-2021 by the Sphinx team, see AUTHORS.
    :license: BSD, see LICENSE for details.
"""

import warnings
from typing import Any, Dict, List, Tuple

from docutils import nodes, utils
from docutils.nodes import Node, system_message
from docutils.parsers.rst.states import Inliner

import sphinx
from sphinx.application import Sphinx
from sphinx.deprecation import RemovedInSphinx60Warning
from sphinx.util.nodes import split_explicit_title
from sphinx.util.typing import RoleFunction


def make_link_role(name: str, base_url: str, caption: str) -> RoleFunction:
    # Check whether we have base_url and caption strings have an '%s' for
    # expansion.  If not, fall back the the old behaviour and use the string as
    # a prefix.
    # Remark: It is an implementation detail that we use Pythons %-formatting.
    # So far we only expose ``%s`` and require quoting of ``%`` using ``%%``.
    try:
        base_url % 'dummy'
    except (TypeError, ValueError):
        warnings.warn('extlinks: Sphinx-6.0 will require base URL to '
                      'contain exactly one \'%s\' and all other \'%\' need '
                      'to be escaped as \'%%\'.', RemovedInSphinx60Warning)
        base_url = base_url.replace('%', '%%') + '%s'
    if caption is not None:
        try:
            caption % 'dummy'
        except (TypeError, ValueError):
            warnings.warn('extlinks: Sphinx-6.0 will require a caption string to '
                          'contain exactly one \'%s\' and all other \'%\' need '
                          'to be escaped as \'%%\'.', RemovedInSphinx60Warning)
            caption = caption.replace('%', '%%') + '%s'

    def role(typ: str, rawtext: str, text: str, lineno: int,
             inliner: Inliner, options: Dict = {}, content: List[str] = []
             ) -> Tuple[List[Node], List[system_message]]:
        text = utils.unescape(text)
        has_explicit_title, title, part = split_explicit_title(text)
        full_url = base_url % part
        if not has_explicit_title:
            if caption is None:
                title = full_url
            else:
                title = caption % part
        pnode = nodes.reference(title, title, internal=False, refuri=full_url)
        return [pnode], []
    return role


def setup_link_roles(app: Sphinx) -> None:
    for name, (base_url, caption) in app.config.extlinks.items():
        app.add_role(name, make_link_role(name, base_url, caption))


def setup(app: Sphinx) -> Dict[str, Any]:
    app.add_config_value('extlinks', {}, 'env')
    app.connect('builder-inited', setup_link_roles)
    return {'version': sphinx.__display_version__, 'parallel_read_safe': True}
