import logging
from common import STYLE, estimate_text_size
import github_data

logging.basicConfig(level=logging.DEBUG, format='[%(levelname)s] %(message)s')

def generate_status_svg(label_text, label_color, count):
    width = 130
    height = 110
    label_text_width, _ = estimate_text_size(label_text)
    label_width = label_text_width + 30

    svg = f"""
    <svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">
        {STYLE}
        <!-- Card Background -->
        <rect class="card" x="0" y="0" width="{width}" height="{height}"/>

        <!-- Label -->
        <g transform="translate({width/2}, 20)">
            <rect class="labelBg" x="{-label_width/2}" y="0" width="{label_width}" height="24" rx="12" fill="{label_color}" stroke="{label_color}" stroke-width="0.5"/>
            <text class="label" x="0" y="17" font-weight="bold" fill="{label_color}" text-anchor="middle">{label_text}</text>
        </g>

        <!-- Count -->
        <text class="counter" x="{width/2}" y="90">{count}</text>
    </svg>
    """
    return svg

if __name__ == "__main__":
    status_labels = github_data.fetch_status_labels()
    for label in status_labels:
        svg = generate_status_svg(label.name, label.color, label.issue_count)

        # Save each SVG to a unique file
        filename = f"status_{label.name.split(':')[1]}.svg"
        with open(filename, "w") as f:
            f.write(svg)
        logging.info(f"Saved SVG as {filename}")
