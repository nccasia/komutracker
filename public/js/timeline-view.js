import { clipEventToRange, isActiveEvent } from './activity-event.js';
import { createElement } from './dom.js';
import { formatDuration, formatEventDateTime } from './format.js';
import { getLocale, t } from './i18n.js';

const MIN_ZOOM = 1;
const MAX_ZOOM = 4;
const ZOOM_STEP = 0.25;
const THREE_HOURS = 3 * 60 * 60 * 1000;
const ONE_DAY = 24 * 60 * 60 * 1000;
const LONG_RANGE_THRESHOLD = 36 * 60 * 60 * 1000;

export function createTimelineView({ viewport, track, timeline, axis, zoomOut, zoomReset, zoomIn }) {
    let zoom = MIN_ZOOM;
    let dragging = false;
    let dragX = 0;

    function updateZoom() {
        zoomReset.textContent = `${Math.round(zoom * 100)}%`;
        track.style.width = `${zoom * 100}%`;
    }

    function setZoom(nextZoom) {
        zoom = Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, nextZoom));
        updateZoom();
    }

    function renderAxis(range) {
        const span = range.end.getTime() - range.start.getTime();
        const longRange = span > LONG_RANGE_THRESHOLD;
        const interval = longRange ? ONE_DAY : THREE_HOURS;
        const tickCount = Math.floor(span / interval);
        const labels = document.createDocumentFragment();

        for (let index = 0; index <= tickCount; index += 1) {
            const offset = Math.min(index * interval, span);
            const point = new Date(range.start.getTime() + offset);
            const text = longRange
                ? point.toLocaleDateString(getLocale(), { month: 'short', day: 'numeric' })
                : point.toLocaleTimeString(getLocale(), { hour: '2-digit', minute: '2-digit' });
            const label = createElement('span', { text });
            label.style.left = `${(offset / span) * 100}%`;
            labels.append(label);
        }

        axis.replaceChildren(labels);
        timeline.style.backgroundSize = `${(interval / span) * 100}% 100%`;
    }

    function createSegment(event, range) {
        const rangeStart = range.start.getTime();
        const rangeEnd = range.end.getTime();
        const { from, to } = clipEventToRange(event, range);
        if (to <= from) return null;

        const isActive = isActiveEvent(event);
        const status = t(isActive ? 'timeline.active' : 'timeline.afk');
        const segment = createElement('div', {
            className: `segment ${isActive ? 'active' : 'afk'}`,
        });
        const span = rangeEnd - rangeStart;
        const eventStart = formatEventDateTime(new Date(event.startAt), getLocale());
        const eventEnd = formatEventDateTime(new Date(event.endAt), getLocale());

        segment.style.left = `${((from - rangeStart) / span) * 100}%`;
        segment.style.width = `${((to - from) / span) * 100}%`;
        segment.title = `${status} · ${formatDuration((to - from) / 1000)}\n${eventStart} – ${eventEnd}`;
        segment.setAttribute('aria-label', t('timeline.segmentLabel', {
            status,
            start: eventStart,
            end: eventEnd,
        }));
        return segment;
    }

    function render(events, range) {
        const segments = document.createDocumentFragment();
        events.forEach((event) => {
            const segment = createSegment(event, range);
            if (segment) segments.append(segment);
        });

        timeline.replaceChildren(segments);
        viewport.scrollLeft = 0;
        updateZoom();
        renderAxis(range);
    }

    zoomOut.addEventListener('click', () => setZoom(zoom - ZOOM_STEP));
    zoomIn.addEventListener('click', () => setZoom(zoom + ZOOM_STEP));
    zoomReset.addEventListener('click', () => setZoom(MIN_ZOOM));

    viewport.addEventListener('wheel', (event) => {
        event.preventDefault();
        const direction = event.deltaY < 0 ? 1 : -1;
        const nextZoom = Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, zoom + direction * ZOOM_STEP));
        if (nextZoom === zoom) return;

        const bounds = viewport.getBoundingClientRect();
        const offsetX = event.clientX - bounds.left;
        const anchor = viewport.scrollLeft + offsetX;
        const previousWidth = track.scrollWidth;
        setZoom(nextZoom);
        const scale = track.scrollWidth / previousWidth;
        viewport.scrollLeft = Math.max(0, anchor * scale - offsetX);
    }, { passive: false });

    viewport.addEventListener('pointerdown', (event) => {
        dragging = true;
        dragX = event.clientX;
        viewport.classList.add('dragging');
        viewport.setPointerCapture(event.pointerId);
    });
    viewport.addEventListener('pointermove', (event) => {
        if (!dragging) return;
        viewport.scrollLeft -= event.clientX - dragX;
        dragX = event.clientX;
    });

    function stopDragging() {
        dragging = false;
        viewport.classList.remove('dragging');
    }

    viewport.addEventListener('pointerup', stopDragging);
    viewport.addEventListener('pointercancel', stopDragging);

    return { render };
}
