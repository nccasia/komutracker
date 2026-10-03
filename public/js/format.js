export function formatDuration(seconds) {
    const total = Math.max(0, Math.round(seconds));
    const hours = String(Math.floor(total / 3600)).padStart(2, '0');
    const minutes = String(Math.floor((total % 3600) / 60)).padStart(2, '0');
    const remaining = String(total % 60).padStart(2, '0');
    return `${hours}:${minutes}:${remaining}`;
}

export function formatEventTime(date) {
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

export function formatEventDateTime(date) {
    return date.toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' });
}
