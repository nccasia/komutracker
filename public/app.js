import { fetchActivityEvents } from './js/activity-api.js';
import { createDateRangeController } from './js/date-range.js';
import { getRequiredElement } from './js/dom.js';
import { readLookupParams, persistLookupParams } from './js/lookup-params.js';
import { createReportView } from './js/report-view.js';
import { createTimelineView } from './js/timeline-view.js';

const ui = {
    form: getRequiredElement('lookup-form'),
    hostname: getRequiredElement('hostname'),
    rangeOptions: document.querySelectorAll('.range-option'),
    customRange: getRequiredElement('custom-range'),
    customStart: getRequiredElement('custom-start'),
    customEnd: getRequiredElement('custom-end'),
    message: getRequiredElement('message'),
    report: getRequiredElement('report'),
};

const rangeController = createDateRangeController({
    options: ui.rangeOptions,
    customContainer: ui.customRange,
    customStart: ui.customStart,
    customEnd: ui.customEnd,
});

const timelineView = createTimelineView({
    viewport: getRequiredElement('timeline-viewport'),
    track: getRequiredElement('timeline-track'),
    timeline: getRequiredElement('timeline'),
    axis: getRequiredElement('timeline-axis'),
    zoomOut: getRequiredElement('zoom-out'),
    zoomReset: getRequiredElement('zoom-reset'),
    zoomIn: getRequiredElement('zoom-in'),
});

const reportView = createReportView({
    container: ui.report,
    title: getRequiredElement('report-title'),
    timespan: getRequiredElement('report-timespan'),
    activeTime: getRequiredElement('active-time'),
    afkTime: getRequiredElement('afk-time'),
    coverage: getRequiredElement('coverage'),
    events: getRequiredElement('events'),
    eventCount: getRequiredElement('event-count'),
    timelineView,
});

function setMessage(text, type = '') {
    ui.message.textContent = text;
    ui.message.className = ['message', type].filter(Boolean).join(' ');
}

function getErrorMessage(error) {
    if (error instanceof Error && error.message === 'User not found') {
        return 'User not found. Check the hostname and try again.';
    }
    return error instanceof Error ? error.message : 'Unable to load this report';
}

async function lookup(event) {
    event.preventDefault();

    const username = ui.hostname.value.trim();
    if (!username) return;

    try {
        const range = rangeController.getSelectedRange();
        persistLookupParams({ username, ...rangeController.getQueryState() });

        setMessage('Reading activity…', 'loading');
        reportView.hide();

        const events = await fetchActivityEvents(username, range);
        reportView.render({ username, range, events });
        setMessage('');
    } catch (error) {
        setMessage(getErrorMessage(error), 'error');
    }
}

function initialize() {
    const params = readLookupParams();
    rangeController.restore(params);

    if (params.username) {
        ui.hostname.value = params.username;
        ui.form.requestSubmit();
    }
}

ui.form.addEventListener('submit', lookup);
initialize();
