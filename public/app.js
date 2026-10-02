const form = document.querySelector('#lookup-form');
const hostnameInput = document.querySelector('#hostname');
const rangeOptions = document.querySelectorAll('.range-option');
const customRange = document.querySelector('#custom-range');
const customStart = document.querySelector('#custom-start');
const customEnd = document.querySelector('#custom-end');
const timelineViewport = document.querySelector('#timeline-viewport');
const timelineAxis = document.querySelector('#timeline-axis');
const zoomOut = document.querySelector('#zoom-out');
const zoomReset = document.querySelector('#zoom-reset');
const zoomIn = document.querySelector('#zoom-in');
const message = document.querySelector('#message');
const report = document.querySelector('#report');
const reportTitle = document.querySelector('#report-title');
const reportEmail = document.querySelector('#report-email');
const activeTime = document.querySelector('#active-time');
const afkTime = document.querySelector('#afk-time');
const coverage = document.querySelector('#coverage');
const timeline = document.querySelector('#timeline');
const eventsList = document.querySelector('#events');
const eventCount = document.querySelector('#event-count');

let rangeMode = 'today';
let timelineZoom = 1;

function localDay(date = new Date()) {
    return new Date(date.getTime() + 7 * 60 * 60 * 1000).toISOString().slice(0, 10);
}

function shiftDay(day, amount) {
    const date = new Date(`${day}T00:00:00+07:00`);
    date.setUTCDate(date.getUTCDate() + amount);
    return localDay(date);
}

const today = localDay();
customStart.value = shiftDay(today, -1);
customEnd.value = today;

function dayRange(day) {
    const start = new Date(`${day}T00:00:00+07:00`);
    return { start, end: new Date(start.getTime() + 86400000) };
}

function selectedRange() {
    if (rangeMode === 'today') return { ...dayRange(today), label: today };
    if (rangeMode === 'yesterday') {
        const day = shiftDay(today, -1);
        return { ...dayRange(day), label: day };
    }
    if (rangeMode === '7days') {
        const startDay = shiftDay(today, -6);
        return { start: dayRange(startDay).start, end: dayRange(today).end, label: `${startDay} – ${today}` };
    }
    if (!customStart.value || !customEnd.value || customEnd.value < customStart.value) {
        throw new Error('Choose a valid custom start and end date.');
    }
    return { start: dayRange(customStart.value).start, end: dayRange(customEnd.value).end, label: `${customStart.value} – ${customEnd.value}` };
}

rangeOptions.forEach((option) => {
    option.addEventListener('click', () => {
        rangeMode = option.dataset.range;
        rangeOptions.forEach((item) => item.classList.toggle('active', item === option));
        customRange.hidden = rangeMode !== 'custom';
    });
});

function formatDuration(seconds) {
    const total = Math.max(0, Math.round(seconds));
    const hours = String(Math.floor(total / 3600)).padStart(2, '0');
    const minutes = String(Math.floor((total % 3600) / 60)).padStart(2, '0');
    const remaining = String(total % 60).padStart(2, '0');
    return `${hours}:${minutes}:${remaining}`;
}

function setMessage(text, type = '') {
    message.textContent = text;
    message.className = `message ${type}`;
}

function clippedSeconds(event, start, end) {
    const from = Math.max(new Date(event.startAt).getTime(), start.getTime());
    const to = Math.min(new Date(event.endAt).getTime(), end.getTime());
    return Math.max(0, (to - from) / 1000);
}

function renderTimeline(events, start, end) {
    timeline.innerHTML = '';
    timeline.style.width = `${timelineZoom * 100}%`;
    timelineViewport.scrollLeft = 0;
    renderAxis(start, end);
    const span = end.getTime() - start.getTime();
    events.forEach((event) => {
        const from = Math.max(new Date(event.startAt).getTime(), start.getTime());
        const to = Math.min(new Date(event.endAt).getTime(), end.getTime());
        if (to <= from) return;
        const segment = document.createElement('div');
        segment.className = `segment ${event.status === 'not-afk' ? 'active' : 'afk'}`;
        segment.style.left = `${((from - start.getTime()) / span) * 100}%`;
        segment.style.width = `${((to - from) / span) * 100}%`;
        const eventStart = new Date(event.startAt).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' });
        const eventEnd = new Date(event.endAt).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' });
        segment.title = `${event.status === 'not-afk' ? 'Active' : 'AFK'} · ${formatDuration((to - from) / 1000)}\n${eventStart} – ${eventEnd}`;
        segment.setAttribute('aria-label', `${event.status === 'not-afk' ? 'Active' : 'AFK'} from ${eventStart} to ${eventEnd}`);
        timeline.appendChild(segment);
    });
}

function renderAxis(start, end) {
    const span = end.getTime() - start.getTime();
    const longRange = span > 36 * 60 * 60 * 1000;
    timelineAxis.innerHTML = '';
    for (let index = 0; index <= 4; index += 1) {
        const point = new Date(start.getTime() + (span * index) / 4);
        const label = longRange
            ? point.toLocaleDateString([], { month: 'short', day: 'numeric' })
            : point.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        const item = document.createElement('span');
        item.textContent = label;
        timelineAxis.appendChild(item);
    }
}

function updateZoom() {
    zoomReset.textContent = `${Math.round(timelineZoom * 100)}%`;
    timeline.style.width = `${timelineZoom * 100}%`;
}

zoomOut.addEventListener('click', () => {
    timelineZoom = Math.max(1, timelineZoom - 0.25);
    updateZoom();
});
zoomIn.addEventListener('click', () => {
    timelineZoom = Math.min(4, timelineZoom + 0.25);
    updateZoom();
});
zoomReset.addEventListener('click', () => {
    timelineZoom = 1;
    updateZoom();
});

timelineViewport.addEventListener('wheel', (event) => {
    event.preventDefault();
    const previousZoom = timelineZoom;
    const direction = event.deltaY < 0 ? 1 : -1;
    const nextZoom = Math.min(4, Math.max(1, previousZoom + direction * 0.25));
    if (nextZoom === previousZoom) return;

    const bounds = timelineViewport.getBoundingClientRect();
    const offsetX = event.clientX - bounds.left;
    const anchor = timelineViewport.scrollLeft + offsetX;
    const previousWidth = timeline.scrollWidth;
    timelineZoom = nextZoom;
    updateZoom();
    const scale = timeline.scrollWidth / previousWidth;
    timelineViewport.scrollLeft = Math.max(0, anchor * scale - offsetX);
}, { passive: false });

let dragging = false;
let dragX = 0;
timelineViewport.addEventListener('pointerdown', (event) => {
    dragging = true;
    dragX = event.clientX;
    timelineViewport.classList.add('dragging');
    timelineViewport.setPointerCapture(event.pointerId);
});
timelineViewport.addEventListener('pointermove', (event) => {
    if (!dragging) return;
    timelineViewport.scrollLeft -= event.clientX - dragX;
    dragX = event.clientX;
});
timelineViewport.addEventListener('pointerup', () => {
    dragging = false;
    timelineViewport.classList.remove('dragging');
});
timelineViewport.addEventListener('pointercancel', () => {
    dragging = false;
    timelineViewport.classList.remove('dragging');
});

function renderEvents(events, start, end) {
    eventsList.innerHTML = '';
    eventCount.textContent = `${events.length} ${events.length === 1 ? 'event' : 'events'}`;
    if (!events.length) {
        eventsList.innerHTML = '<div class="empty">No tracked activity for this day.</div>';
        return;
    }
    events.forEach((event) => {
        const duration = clippedSeconds(event, start, end);
        const row = document.createElement('div');
        row.className = 'event-row';
        row.innerHTML = `<div class="event-status"><i class="dot ${event.status === 'not-afk' ? 'dot-active' : 'dot-afk'}"></i>${event.status === 'not-afk' ? 'Active' : 'AFK'}</div><div class="event-time">${new Date(event.startAt).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })} – ${new Date(event.endAt).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}</div><div class="event-duration">${formatDuration(duration)}</div>`;
        eventsList.appendChild(row);
    });
}

async function lookup(event) {
    event.preventDefault();
    const hostname = hostnameInput.value.trim();
    if (!hostname) return;
    setMessage('Reading activity…', 'loading');
    report.hidden = true;
    try {
        const { start, end, label } = selectedRange();
        const url = `/api/0/users/${encodeURIComponent(hostname)}/events?start=${encodeURIComponent(start.toISOString())}&end=${encodeURIComponent(end.toISOString())}`;
        const response = await fetch(url);
        if (response.status === 404) throw new Error('User not found');
        if (!response.ok) throw new Error('Unable to load this report');
        const events = await response.json();
        const activeSeconds = events.reduce((sum, item) => sum + (item.status === 'not-afk' ? clippedSeconds(item, start, end) : 0), 0);
        const afkSeconds = events.reduce((sum, item) => sum + (item.status === 'afk' ? clippedSeconds(item, start, end) : 0), 0);
        const total = (end - start) / 1000;
        reportTitle.textContent = hostname;
        reportEmail.textContent = `Timespan · ${label}`;
        activeTime.textContent = formatDuration(activeSeconds);
        afkTime.textContent = formatDuration(afkSeconds);
        coverage.textContent = `${Math.min(100, Math.round(((activeSeconds + afkSeconds) / total) * 100))}%`;
        renderTimeline(events, start, end);
        renderEvents(events, start, end);
        report.hidden = false;
        setMessage('');
    } catch (error) {
        setMessage(error.message === 'User not found' ? 'User not found. Check the hostname and try again.' : error.message, 'error');
    }
}

form.addEventListener('submit', lookup);

const username = new URLSearchParams(window.location.search).get('username')?.trim();
if (username) {
    hostnameInput.value = username;
    form.requestSubmit();
}
