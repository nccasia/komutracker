import { fetchActivityEvents } from './js/activity-api.js';
import { createDateRangeController } from './js/date-range.js';
import { getRequiredElement } from './js/dom.js';
import { initializeI18n, t } from './js/i18n.js';
import { redirectLegacyActivityUrl } from './js/legacy-url.js';
import { readLookupParams, persistLookupParams } from './js/lookup-params.js';
import { createReportView } from './js/report-view.js';
import { createTimelineView } from './js/timeline-view.js';

initializeI18n();

const ui = {
    form: getRequiredElement('lookup-form'),
    hostname: getRequiredElement('hostname'),
    rangeOptions: document.querySelectorAll('.range-option'),
    customRange: getRequiredElement('custom-range'),
    customStart: getRequiredElement('custom-start'),
    customEnd: getRequiredElement('custom-end'),
    customWarning: getRequiredElement('custom-range-warning'),
    message: getRequiredElement('message'),
    report: getRequiredElement('report'),
};

const rangeController = createDateRangeController({
    options: ui.rangeOptions,
    customContainer: ui.customRange,
    customStart: ui.customStart,
    customEnd: ui.customEnd,
    customWarning: ui.customWarning,
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

let messageTranslation = null;

function setMessage(text, type = '', translation = null) {
    ui.message.textContent = text;
    ui.message.className = ['message', type].filter(Boolean).join(' ');
    messageTranslation = translation;
}

function getErrorMessage(error) {
    if (error instanceof Error && error.translationKey) {
        return {
            text: t(error.translationKey, error.translationValues),
            translation: { key: error.translationKey, values: error.translationValues },
        };
    }
    if (error instanceof Error && error.message === 'User not found') {
        return { text: t('error.userNotFound'), translation: { key: 'error.userNotFound' } };
    }
    if (error instanceof Error && error.message === 'Unable to load this report') {
        return { text: t('error.unableToLoad'), translation: { key: 'error.unableToLoad' } };
    }
    if (error instanceof Error && error.message) return { text: error.message };
    return { text: t('error.unableToLoad'), translation: { key: 'error.unableToLoad' } };
}

async function lookup(event) {
    event.preventDefault();

    const username = ui.hostname.value.trim();
    if (!username) return;

    try {
        const range = rangeController.getSelectedRange();
        persistLookupParams({ username, ...rangeController.getQueryState() });

        setMessage(t('message.loading'), 'loading', { key: 'message.loading' });
        reportView.hide();

        const events = await fetchActivityEvents(username, range);
        reportView.render({ username, range, events });
        setMessage('');
    } catch (error) {
        const message = getErrorMessage(error);
        setMessage(message.text, 'error', message.translation);
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
document.addEventListener('languagechange', () => {
    rangeController.refreshLanguage();
    reportView.refreshLanguage();
    if (messageTranslation) {
        setMessage(
            t(messageTranslation.key, messageTranslation.values),
            ui.message.classList.contains('error') ? 'error' : 'loading',
            messageTranslation,
        );
    }
});
if (!redirectLegacyActivityUrl()) initialize();
