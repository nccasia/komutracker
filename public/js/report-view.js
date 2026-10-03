import { AFK_STATUS, clipEventToRange, isActiveEvent } from './activity-event.js';
import { createElement } from './dom.js';
import { formatDuration, formatEventTime } from './format.js';
import { getLocale, t } from './i18n.js';

function summarizeEvents(events, range) {
    const totals = events.reduce((summary, event) => {
        const seconds = clipEventToRange(event, range).seconds;
        if (isActiveEvent(event)) summary.active += seconds;
        if (event.status === AFK_STATUS) summary.afk += seconds;
        return summary;
    }, { active: 0, afk: 0 });

    const rangeSeconds = (range.end - range.start) / 1000;
    const trackedPercentage = rangeSeconds > 0
        ? Math.min(100, Math.round(((totals.active + totals.afk) / rangeSeconds) * 100))
        : 0;

    return { ...totals, trackedPercentage };
}

function createEventRow(event, range) {
    const isActive = isActiveEvent(event);
    const row = createElement('div', { className: 'event-row' });
    const status = createElement('div', { className: 'event-status' });
    const dot = createElement('i', { className: `dot ${isActive ? 'dot-active' : 'dot-afk'}` });
    const start = new Date(event.startAt);
    const end = new Date(event.endAt);

    status.append(dot, document.createTextNode(t(isActive ? 'timeline.active' : 'timeline.afk')));
    row.append(
        status,
        createElement('div', {
            className: 'event-time',
            text: `${formatEventTime(start, getLocale())} – ${formatEventTime(end, getLocale())}`,
        }),
        createElement('div', {
            className: 'event-duration',
            text: formatDuration(clipEventToRange(event, range).seconds),
        }),
    );
    return row;
}

export function createReportView(elements) {
    let lastReport;

    function hide() {
        elements.container.hidden = true;
    }

    function renderEvents(events, range) {
        elements.events.replaceChildren();
        elements.eventCount.textContent = t(
            events.length === 1 ? 'events.countOne' : 'events.countOther',
            { count: events.length },
        );

        if (!events.length) {
            elements.events.append(createElement('div', {
                className: 'empty',
                text: t('events.empty'),
            }));
            return;
        }

        const fragment = document.createDocumentFragment();
        events.forEach((event) => fragment.append(createEventRow(event, range)));
        elements.events.append(fragment);
    }

    function render({ username, range, events }) {
        lastReport = { username, range, events };
        const summary = summarizeEvents(events, range);

        elements.title.textContent = username;
        elements.timespan.textContent = t('report.timespan', { range: range.label });
        elements.activeTime.textContent = formatDuration(summary.active);
        elements.afkTime.textContent = formatDuration(summary.afk);
        elements.coverage.textContent = `${summary.trackedPercentage}%`;
        elements.timelineView.render(events, range);
        renderEvents(events, range);
        elements.container.hidden = false;
    }

    function refreshLanguage() {
        if (lastReport && !elements.container.hidden) render(lastReport);
    }

    return { hide, render, refreshLanguage };
}
