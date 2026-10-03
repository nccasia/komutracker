const STORAGE_KEY = 'komutracker-language';

const TRANSLATIONS = {
    en: {
        'language.selector': 'Select language',
        'brand.home': 'Komutracker home',
        'hero.eyebrow': 'Personal pulse',
        'hero.description': 'Look up a username to review active and AFK stretches across the day.',
        'form.username': 'Username',
        'form.usernamePlaceholder': 'e.g. ngocanh',
        'form.timespan': 'Timespan',
        'form.selectTimespan': 'Select timespan',
        'range.today': 'Today',
        'range.yesterday': 'Yesterday',
        'range.sevenDays': '7 days',
        'range.custom': 'Custom',
        'range.start': 'Start',
        'range.end': 'End',
        'range.warning': 'This custom range covers {days} days. Reports over 14 days may take longer to load.',
        'range.invalid': 'Choose a valid custom start and end date.',
        'range.outOfBounds': 'Custom dates must be within the last {days} days.',
        'form.search': 'Search report',
        'message.loading': 'Reading activity…',
        'error.userNotFound': 'User not found. Check the hostname and try again.',
        'error.unableToLoad': 'Unable to load this report',
        'report.eyebrow': 'Daily snapshot',
        'report.timespan': 'Timespan · {range}',
        'metric.active': 'Active time',
        'metric.activeHint': 'keyboard and presence',
        'metric.afk': 'AFK time',
        'metric.afkHint': 'away from input',
        'metric.tracked': 'Tracked',
        'metric.trackedHint': 'of the selected timespan',
        'timeline.eyebrow': 'Day arc',
        'timeline.title': 'Activity timeline',
        'timeline.active': 'Active',
        'timeline.afk': 'AFK',
        'timeline.controls': 'Timeline zoom controls',
        'timeline.zoomOut': 'Zoom out',
        'timeline.resetZoom': 'Reset zoom',
        'timeline.zoomIn': 'Zoom in',
        'timeline.label': 'Activity timeline',
        'timeline.segmentLabel': '{status} from {start} to {end}',
        'events.eyebrow': 'Raw intervals',
        'events.title': 'Event detail',
        'events.countOne': '{count} event',
        'events.countOther': '{count} events',
        'events.empty': 'No tracked activity for this timespan.',
        'footer.text': 'Built for a quieter view of work rhythm. No window titles are collected.',
    },
    vi: {
        'language.selector': 'Chọn ngôn ngữ',
        'brand.home': 'Trang chủ Komutracker',
        'hero.eyebrow': 'Nhịp làm việc cá nhân',
        'hero.description': 'Tra cứu tên người dùng để xem các khoảng hoạt động và AFK trong ngày.',
        'form.username': 'Tên người dùng',
        'form.usernamePlaceholder': 'ví dụ: ngocanh',
        'form.timespan': 'Khoảng thời gian',
        'form.selectTimespan': 'Chọn khoảng thời gian',
        'range.today': 'Hôm nay',
        'range.yesterday': 'Hôm qua',
        'range.sevenDays': '7 ngày',
        'range.custom': 'Tùy chỉnh',
        'range.start': 'Bắt đầu',
        'range.end': 'Kết thúc',
        'range.warning': 'Khoảng tùy chỉnh này kéo dài {days} ngày. Báo cáo trên 14 ngày có thể cần nhiều thời gian hơn để tải.',
        'range.invalid': 'Vui lòng chọn ngày bắt đầu và ngày kết thúc hợp lệ.',
        'range.outOfBounds': 'Ngày tùy chỉnh phải nằm trong {days} ngày gần nhất.',
        'form.search': 'Xem báo cáo',
        'message.loading': 'Đang tải hoạt động…',
        'error.userNotFound': 'Không tìm thấy người dùng. Hãy kiểm tra tên máy và thử lại.',
        'error.unableToLoad': 'Không thể tải báo cáo này',
        'report.eyebrow': 'Tổng quan hằng ngày',
        'report.timespan': 'Khoảng thời gian · {range}',
        'metric.active': 'Thời gian hoạt động',
        'metric.activeHint': 'bàn phím và trạng thái hiện diện',
        'metric.afk': 'Thời gian AFK',
        'metric.afkHint': 'không có thao tác',
        'metric.tracked': 'Đã ghi nhận',
        'metric.trackedHint': 'trong khoảng thời gian đã chọn',
        'timeline.eyebrow': 'Nhịp trong ngày',
        'timeline.title': 'Dòng thời gian hoạt động',
        'timeline.active': 'Hoạt động',
        'timeline.afk': 'AFK',
        'timeline.controls': 'Điều khiển thu phóng dòng thời gian',
        'timeline.zoomOut': 'Thu nhỏ',
        'timeline.resetZoom': 'Đặt lại mức thu phóng',
        'timeline.zoomIn': 'Phóng to',
        'timeline.label': 'Dòng thời gian hoạt động',
        'timeline.segmentLabel': '{status} từ {start} đến {end}',
        'events.eyebrow': 'Các khoảng ghi nhận',
        'events.title': 'Chi tiết sự kiện',
        'events.countOne': '{count} sự kiện',
        'events.countOther': '{count} sự kiện',
        'events.empty': 'Không có hoạt động nào được ghi nhận trong khoảng thời gian này.',
        'footer.text': 'Một góc nhìn yên tĩnh hơn về nhịp làm việc. Không thu thập tiêu đề cửa sổ.',
    },
};

let language = 'en';

function interpolate(message, values) {
    return Object.entries(values).reduce(
        (result, [key, value]) => result.replaceAll(`{${key}}`, String(value)),
        message,
    );
}

export function t(key, values = {}) {
    const message = TRANSLATIONS[language]?.[key] ?? TRANSLATIONS.en[key] ?? key;
    return interpolate(message, values);
}

export function getLanguage() {
    return language;
}

export function getLocale() {
    return language === 'vi' ? 'vi-VN' : 'en-US';
}

function translatePage(root = document) {
    root.querySelectorAll('[data-i18n]').forEach((element) => {
        element.textContent = t(element.dataset.i18n);
    });
    root.querySelectorAll('[data-i18n-placeholder]').forEach((element) => {
        element.setAttribute('placeholder', t(element.dataset.i18nPlaceholder));
    });
    root.querySelectorAll('[data-i18n-aria-label]').forEach((element) => {
        element.setAttribute('aria-label', t(element.dataset.i18nAriaLabel));
    });
    root.querySelectorAll('[data-i18n-title]').forEach((element) => {
        element.setAttribute('title', t(element.dataset.i18nTitle));
    });
}

function updateLanguageControls() {
    document.querySelectorAll('[data-language]').forEach((option) => {
        const isActive = option.dataset.language === language;
        option.classList.toggle('active', isActive);
        option.setAttribute('aria-pressed', String(isActive));
    });
}

export function setLanguage(nextLanguage, { persist = true, notify = true } = {}) {
    if (!Object.hasOwn(TRANSLATIONS, nextLanguage)) return;

    language = nextLanguage;
    document.documentElement.lang = language;
    translatePage();
    updateLanguageControls();

    if (persist) {
        try {
            localStorage.setItem(STORAGE_KEY, language);
        } catch {
            // The UI still works when storage is unavailable.
        }
    }
    if (notify) document.dispatchEvent(new CustomEvent('languagechange'));
}

export function initializeI18n() {
    let savedLanguage;
    try {
        savedLanguage = localStorage.getItem(STORAGE_KEY);
    } catch {
        // Use the default language when storage is unavailable.
    }

    setLanguage(savedLanguage || 'en', { persist: false, notify: false });
    document.querySelectorAll('[data-language]').forEach((option) => {
        option.addEventListener('click', () => setLanguage(option.dataset.language));
    });
}
