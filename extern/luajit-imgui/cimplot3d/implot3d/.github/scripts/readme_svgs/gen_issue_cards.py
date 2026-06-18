import logging
import requests
import base64
import dataclasses
from datetime import datetime
import common
import github_data
from github_data import IssueType, IssueState, PrState

# Number of issue cards to be generated
ISSUE_COUNT = 6

# Icons extracted from https://primer.style/octicons/
ISSUE_OPEN_16 = "M8 9.5a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3Z M8 0a8 8 0 1 1 0 16A8 8 0 0 1 8 0ZM1.5 8a6.5 6.5 0 1 0 13 0 6.5 6.5 0 0 0-13 0Z"
ISSUE_CLOSED_16 = "M8 0a8 8 0 1 1 0 16A8 8 0 0 1 8 0ZM1.5 8a6.5 6.5 0 1 0 13 0 6.5 6.5 0 0 0-13 0Zm9.78-2.22-5.5 5.5a.749.749 0 0 1-1.275-.326.749.749 0 0 1 .215-.734l5.5-5.5a.751.751 0 0 1 1.042.018.751.751 0 0 1 .018 1.042Z"
ISSUE_COMPLETED_16 = "M11.28 6.78a.75.75 0 0 0-1.06-1.06L7.25 8.69 5.78 7.22a.75.75 0 0 0-1.06 1.06l2 2a.75.75 0 0 0 1.06 0l3.5-3.5Z M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0Zm-1.5 0a6.5 6.5 0 1 0-13 0 6.5 6.5 0 0 0 13 0Z"
BUG_16 = "M4.72.22a.75.75 0 0 1 1.06 0l1 .999a3.488 3.488 0 0 1 2.441 0l.999-1a.748.748 0 0 1 1.265.332.75.75 0 0 1-.205.729l-.775.776c.616.63.995 1.493.995 2.444v.327c0 .1-.009.197-.025.292.408.14.764.392 1.029.722l1.968-.787a.75.75 0 0 1 .556 1.392L13 7.258V9h2.25a.75.75 0 0 1 0 1.5H13v.5c0 .409-.049.806-.141 1.186l2.17.868a.75.75 0 0 1-.557 1.392l-2.184-.873A4.997 4.997 0 0 1 8 16a4.997 4.997 0 0 1-4.288-2.427l-2.183.873a.75.75 0 0 1-.558-1.392l2.17-.868A5.036 5.036 0 0 1 3 11v-.5H.75a.75.75 0 0 1 0-1.5H3V7.258L.971 6.446a.75.75 0 0 1 .558-1.392l1.967.787c.265-.33.62-.583 1.03-.722a1.677 1.677 0 0 1-.026-.292V4.5c0-.951.38-1.814.995-2.444L4.72 1.28a.75.75 0 0 1 0-1.06Zm.53 6.28a.75.75 0 0 0-.75.75V11a3.5 3.5 0 1 0 7 0V7.25a.75.75 0 0 0-.75-.75ZM6.173 5h3.654A.172.172 0 0 0 10 4.827V4.5a2 2 0 1 0-4 0v.327c0 .096.077.173.173.173Z"
PIN_16 = "m11.294.984 3.722 3.722a1.75 1.75 0 0 1-.504 2.826l-1.327.613a3.089 3.089 0 0 0-1.707 2.084l-.584 2.454c-.317 1.332-1.972 1.8-2.94.832L5.75 11.311 1.78 15.28a.749.749 0 1 1-1.06-1.06l3.969-3.97-2.204-2.204c-.968-.968-.5-2.623.832-2.94l2.454-.584a3.08 3.08 0 0 0 2.084-1.707l.613-1.327a1.75 1.75 0 0 1 2.826-.504ZM6.283 9.723l2.732 2.731a.25.25 0 0 0 .42-.119l.584-2.454a4.586 4.586 0 0 1 2.537-3.098l1.328-.613a.25.25 0 0 0 .072-.404l-3.722-3.722a.25.25 0 0 0-.404.072l-.613 1.328a4.584 4.584 0 0 1-3.098 2.537l-2.454.584a.25.25 0 0 0-.119.42l2.731 2.732Z"
COMMIT_16 = "M11.93 8.5a4.002 4.002 0 0 1-7.86 0H.75a.75.75 0 0 1 0-1.5h3.32a4.002 4.002 0 0 1 7.86 0h3.32a.75.75 0 0 1 0 1.5Zm-1.43-.75a2.5 2.5 0 1 0-5 0 2.5 2.5 0 0 0 5 0Z"
BRANCH_16 = "M9.5 3.25a2.25 2.25 0 1 1 3 2.122V6A2.5 2.5 0 0 1 10 8.5H6a1 1 0 0 0-1 1v1.128a2.251 2.251 0 1 1-1.5 0V5.372a2.25 2.25 0 1 1 1.5 0v1.836A2.493 2.493 0 0 1 6 7h4a1 1 0 0 0 1-1v-.628A2.25 2.25 0 0 1 9.5 3.25Zm-6 0a.75.75 0 1 0 1.5 0 .75.75 0 0 0-1.5 0Zm8.25-.75a.75.75 0 1 0 0 1.5.75.75 0 0 0 0-1.5ZM4.25 12a.75.75 0 1 0 0 1.5.75.75 0 0 0 0-1.5Z"
PULL_REQUEST_16 = "M1.5 3.25a2.25 2.25 0 1 1 3 2.122v5.256a2.251 2.251 0 1 1-1.5 0V5.372A2.25 2.25 0 0 1 1.5 3.25Zm5.677-.177L9.573.677A.25.25 0 0 1 10 .854V2.5h1A2.5 2.5 0 0 1 13.5 5v5.628a2.251 2.251 0 1 1-1.5 0V5a1 1 0 0 0-1-1h-1v1.646a.25.25 0 0 1-.427.177L7.177 3.427a.25.25 0 0 1 0-.354ZM3.75 2.5a.75.75 0 1 0 0 1.5.75.75 0 0 0 0-1.5Zm0 9.5a.75.75 0 1 0 0 1.5.75.75 0 0 0 0-1.5Zm8.25.75a.75.75 0 1 0 1.5 0 .75.75 0 0 0-1.5 0Z"
PULL_REQUEST_MERGED_16 = "M5.45 5.154A4.25 4.25 0 0 0 9.25 7.5h1.378a2.251 2.251 0 1 1 0 1.5H9.25A5.734 5.734 0 0 1 5 7.123v3.505a2.25 2.25 0 1 1-1.5 0V5.372a2.25 2.25 0 1 1 1.95-.218ZM4.25 13.5a.75.75 0 1 0 0-1.5.75.75 0 0 0 0 1.5Zm8.5-4.5a.75.75 0 1 0 0-1.5.75.75 0 0 0 0 1.5ZM5 3.25a.75.75 0 1 0 0 .005V3.25Z"
PULL_REQUEST_CLOSED_16 = "M3.25 1A2.25 2.25 0 0 1 4 5.372v5.256a2.251 2.251 0 1 1-1.5 0V5.372A2.251 2.251 0 0 1 3.25 1Zm9.5 5.5a.75.75 0 0 1 .75.75v3.378a2.251 2.251 0 1 1-1.5 0V7.25a.75.75 0 0 1 .75-.75Zm-2.03-5.273a.75.75 0 0 1 1.06 0l.97.97.97-.97a.748.748 0 0 1 1.265.332.75.75 0 0 1-.205.729l-.97.97.97.97a.751.751 0 0 1-.018 1.042.751.751 0 0 1-1.042.018l-.97-.97-.97.97a.749.749 0 0 1-1.275-.326.749.749 0 0 1 .215-.734l.97-.97-.97-.97a.75.75 0 0 1 0-1.06ZM2.5 3.25a.75.75 0 1 0 1.5 0 .75.75 0 0 0-1.5 0ZM3.25 12a.75.75 0 1 0 0 1.5.75.75 0 0 0 0-1.5Zm9.5 0a.75.75 0 1 0 0 1.5.75.75 0 0 0 0-1.5Z"
COMMENT_16 = "M1 2.75C1 1.784 1.784 1 2.75 1h10.5c.966 0 1.75.784 1.75 1.75v7.5A1.75 1.75 0 0 1 13.25 12H9.06l-2.573 2.573A1.458 1.458 0 0 1 4 13.543V12H2.75A1.75 1.75 0 0 1 1 10.25Zm1.75-.25a.25.25 0 0 0-.25.25v7.5c0 .138.112.25.25.25h2a.75.75 0 0 1 .75.75v2.19l2.72-2.72a.749.749 0 0 1 .53-.22h4.5a.25.25 0 0 0 .25-.25v-7.5a.25.25 0 0 0-.25-.25Z"

logging.basicConfig(level=logging.DEBUG, format='[%(levelname)s] %(message)s')

def fetch_base64_image(url):
    """
    Fetch an image from the given URL and return its Base64-encoded representation.
    """
    response = requests.get(url)
    if response.status_code == 200:
        return base64.b64encode(response.content).decode('utf-8')
    else:
        raise ValueError(f"Failed to fetch image from {url}, status code: {response.status_code}")


def generate_issue_svg(issue):
    width = 670
    height = 100

    card_pad = 8 # Card padding
    int_pad = 4 # Internal padding

    # Avatar info
    avatar_size = 35
    avatar_gap = 20

    # Issue icon
    icon_size = 16
    issue_icon_path = ISSUE_OPEN_16
    if issue.type == IssueType.FEATURE and issue.state == IssueState.COMPLETED:
        issue_icon_path = ISSUE_COMPLETED_16
    elif issue.type == IssueType.FEATURE and issue.state == IssueState.CLOSED:
        issue_icon_path = ISSUE_CLOSED_16
    elif issue.type == IssueType.BUG:
        issue_icon_path = BUG_16
    # Issue icon color
    issue_icon_class = "icon-open"
    if issue.state == IssueState.COMPLETED:
        issue_icon_class = "icon-completed"
    elif issue.state == IssueState.CLOSED:
        issue_icon_class = "icon-closed"

    # Format dates
    created_at_dt = datetime.strptime(issue.created_at, "%Y-%m-%dT%H:%M:%SZ")
    created_at_formatted = f"{created_at_dt.strftime('%b')} {created_at_dt.day}"
    last_commented_at_formatted = None
    if issue.last_commented_at:
        last_commented_at_dt = datetime.strptime(issue.last_commented_at, "%Y-%m-%dT%H:%M:%SZ")
        last_commented_at_formatted = f"{last_commented_at_dt.strftime('%b')} {last_commented_at_dt.day}"

    # Build the contributor text
    contributor_comment =  f'<tspan style="text-decoration: underline;">{issue.author}</tspan> opened on {created_at_formatted}'
    if issue.last_commenter and last_commented_at_formatted:
        contributor_comment = contributor_comment + f' · <tspan style="text-decoration: underline;">{issue.last_commenter}</tspan> commented on {last_commented_at_formatted}'

    svg = f"""
    <svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">
        <!-- {issue.url} -->

        <!-- Definitions -->
        <defs>
            <clipPath id="circle-clip">
                <circle cx="0" cy="0" r="{avatar_size / 2}" />
            </clipPath>
        </defs>

        <!-- Style -->
        {common.STYLE}

        <!-- Card Background -->
        <rect class="card" x="0" y="0" width="{width}" height="{height}"/>

        <!-- Issue Icon -->
        <path class="{issue_icon_class}" transform="translate({card_pad}, {card_pad})" d="{issue_icon_path}"/>

        <!-- Issue Title -->
        <text class="title" x="{card_pad+icon_size+int_pad}" y="{card_pad+icon_size/2+2}" text-anchor="start" dominant-baseline="middle">
            {issue.title[:40] + '...' if len(issue.title) > 40 else issue.title} <tspan style="fill: var(--fgColor-muted);font-weight: var(--text-weight-normal);">#{issue.number}</tspan>
        </text>

        """
    top_right_x = width-card_pad

    # Pinned icon
    if issue.is_pinned:
        top_right_x -= icon_size
        svg += f"""
        <!-- Pinned Icon -->
        <path class="icon-muted" transform="translate({top_right_x}, {card_pad})" d="{PIN_16}"/>

        """

    # Labels
    label_x = top_right_x
    label_y = card_pad
    for label in issue.labels:
        label_color = label.color
        label_text = label.name
        label_width = common.estimate_text_width(label_text, 12) + 15
        label_height = icon_size
        label_x -= label_width + int_pad

        svg += f"""
        <!-- Label -->
        <g transform="translate({label_x + label_width/2}, {label_y})">
            <rect class="labelBg-muted" x="{-label_width/2}" y="0" width="{label_width}" height="{label_height}" rx="{label_height/2}"/>
            <text class="label-muted" x="0" y="{label_height/2+1}" text-anchor="middle" dominant-baseline="middle">{label_text}</text>
        </g>
        """

    # Contributor comment
    svg += f"""
        <!-- Contributor Comment -->
        <text class="small" fill="var(--fgColor-muted)" x="{card_pad}" y="{height-card_pad-1}">{contributor_comment}</text>
    """

    right_footer_x = width - card_pad
    right_footer_y = height - card_pad
    # Deletions
    if issue.deletions is not None:
        text = f"-{issue.deletions}"
        right_footer_x -= common.estimate_text_width(text, 14)
        svg += f"""
            <!-- Deletions -->
            <text class="small" fill="var(--fgColor-danger)" x="{right_footer_x}" y="{right_footer_y}">{text}</text>
            """
    # Additions
    if issue.additions is not None:
        text = f"+{issue.additions}"
        right_footer_x -= common.estimate_text_width(text, 14) + int_pad
        svg += f"""
            <!-- Additions -->
            <text class="small" fill="var(--fgColor-success)" x="{right_footer_x}" y="{right_footer_y}">{text}</text>
            """
    # Number of commits
    if issue.commit_count is not None and issue.commit_count > 0:
        text = f"{issue.commit_count}"
        right_footer_x -= common.estimate_text_width(text, 14) + int_pad
        svg += f"""
            <!-- Commit Count -->
            <text class="small muted" x="{right_footer_x}" y="{right_footer_y}">{text}</text>
            """
        right_footer_x -= 14
        svg += f"""
            <path class="icon-muted" transform="translate({right_footer_x}, {right_footer_y-10}) scale(0.75)" d="{COMMIT_16}"/>
            """
    # Linked PR
    if issue.linked_pr is not None:
        text = f"#{issue.linked_pr}"
        right_footer_x -= common.estimate_text_width(text, 14) + int_pad
        svg += f"""
            <!-- Linked PR -->
            <text class="small muted" x="{right_footer_x}" y="{right_footer_y}">{text}</text>
            """
        pr_path = PULL_REQUEST_16
        path_class = "icon-open"
        if issue.linked_pr_state == PrState.CLOSED:
            pr_path = PULL_REQUEST_CLOSED_16
            path_class = "icon-danger"
        elif issue.linked_pr_state == PrState.MERGED:
            pr_path = PULL_REQUEST_MERGED_16
            path_class = "icon-completed"
        right_footer_x -= 14
        svg += f"""
            <path class="{path_class}" transform="translate({right_footer_x}, {right_footer_y-10}) scale(0.75)" d="{pr_path}"/>
            """
    # Linked branch
    elif issue.linked_branch is not None:
        text = f"{issue.linked_branch}"
        right_footer_x -= common.estimate_text_width(text, 13) + int_pad
        svg += f"""
            <!-- Linked Branch -->
            <text class="small muted" x="{right_footer_x}" y="{right_footer_y}">{text}</text>
            """
        right_footer_x -= 14
        svg += f"""
            <path class="icon-muted" transform="translate({right_footer_x}, {right_footer_y-10}) scale(0.75)" d="{BRANCH_16}"/>
            """

    # Contributor avatars
    middle_right_x = width - card_pad - avatar_size / 2
    middle_y_off = 4
    avatar_y = height / 2 + middle_y_off
    svg += """
        <!-- Avatars -->
        """
    for avatar in issue.contributors_avatars:
        try:
            base64_image = fetch_base64_image(avatar)
            svg += f"""
            <g transform="translate({middle_right_x}, {avatar_y})">
                <image x="{-avatar_size / 2}" y="{-avatar_size / 2}" width="{avatar_size}" height="{avatar_size}" href="data:image/png;base64,{base64_image}" clip-path="url(#circle-clip)" />
                <circle cx="0" cy="0" r="{avatar_size / 2}" fill="none" stroke="var(--border-color)" stroke-width="1" />
            </g>
            """
            middle_right_x -= avatar_gap
        except ValueError as e:
            print(f"Failed to fetch image {avatar}, error: {e}")
    svg += """
        """

    # Comments
    if issue.comment_count > 0:
        text = f"{issue.comment_count}"
        pad = 8
        label_width = icon_size + common.estimate_text_width(text, 14) + pad*2
        label_height = icon_size + int_pad * 1.5
        middle_right_x -= label_width + int_pad

        svg += f"""
        <!-- Comments -->
        <g transform="translate({middle_right_x}, {height / 2 - label_height/2 + middle_y_off})">
            <rect class="labelBg-muted" x="0" y="0" width="{label_width}" height="{label_height}" rx="{label_height/2}"/>
            <path class="icon-muted" transform="translate({pad-1}, {int_pad/2+2}) scale(0.875)" d="{COMMENT_16}"/>
            <text class="small muted" x="{pad+icon_size}" y="15">{issue.comment_count}</text>
        </g>
        """


    # Reactions
    reaction_emoji_map = {
        "plus_one": "👍",
        "minus_one": "👎",
        "laugh": "😄",
        "hooray": "🎉",
        "confused": "🤔",
        "heart": "❤️",
        "rocket": "🚀",
        "eyes": "👀",
    }
    if hasattr(issue, 'reactions') and issue.reactions:
        for reaction_name, count in reversed(dataclasses.asdict(issue.reactions).items()):
            if count > 0:
                emoji = reaction_emoji_map.get(reaction_name)
                if not emoji: # Skip if we don't have an emoji for this reaction
                    continue
                text = f"{count}"
                pad = 8
                label_width = icon_size + common.estimate_text_width(text, 14) + pad*2
                label_height = icon_size + int_pad*1.5
                middle_right_x -= label_width + int_pad

                svg += f"""
                <!-- Comments -->
                <g transform="translate({middle_right_x}, {height / 2 - label_height/2 + middle_y_off})">
                    <rect class="labelBg-muted" x="0" y="0" width="{label_width}" height="{label_height}" rx="{label_height/2}"/>
                    <text class="small muted" x="{pad-1}" y="15">{emoji}</text>
                    <text class="small muted" x="{pad+icon_size}" y="15">{count}</text>
                </g>
                """

    # Progress bar
    if issue.total_tasks:
        progress = issue.completed_tasks / issue.total_tasks
        progress_width = width / 2
        progress_height = 8
        radius = progress_height / 2
        progress_x = card_pad
        progress_y = (height - progress_height) / 2 - 4
        progress_fill_width = max(int(progress_width * progress), radius*2)
        progress_text_y = progress_height + 12

        # Build progress text
        bold_tspan = '<tspan style="font-weight: var(--text-weight-semibold);">'
        tspan_end = '</tspan>'

        percentage = int(progress * 100)
        done = issue.completed_tasks
        todo = issue.total_tasks - issue.completed_tasks

        progress_text = f"""<text class="small muted" x="0" y="{progress_text_y}">"""
        progress_text += f"{bold_tspan}{percentage}%{tspan_end} complete"
        if done > 0:
            progress_text += f" · {bold_tspan}{done}{tspan_end} done"
        if todo > 0:
            progress_text += f" · {bold_tspan}{todo}{tspan_end} to do"
        progress_text += "</text>"

        # Build the progress bar
        svg += f"""
        <!-- Progress Bar -->
        <g transform="translate({progress_x}, {progress_y})">
            <rect class="progress-bg" rx="{radius}" ry="{radius}" x="0" y="0" width="{progress_width}" height="{progress_height}"/>
            <rect class="progress-fill" rx="{radius}" ry="{radius}" x="0" y="0" width="{progress_fill_width}" height="{progress_height}"/>
            {progress_text}
        </g>
        """

    # End SVG
    svg += f"""
    </svg>
    """

    return svg

if __name__ == "__main__":
    issues = github_data.fetch_top_issues(ISSUE_COUNT)
    for i, issue in enumerate(issues):
        logging.info(f"Processing issue: {issue.title}")
        svg = generate_issue_svg(issue)

        # Save each SVG to a unique file
        filename = f"issue_{i}.svg"
        with open(filename, "w") as f:
            f.write(svg)
        logging.info(f"Saved SVG as {filename}")
