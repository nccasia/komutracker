interface CacheItem<T> {
    value: T;
    expiresAt: number;
}

export class TtlCache<T> {
    private readonly values = new Map<string, CacheItem<T>>();

    constructor(private readonly ttlMs: number, private readonly maxEntries: number) {}

    get(key: string): T | undefined {
        const item = this.values.get(key);
        if (!item || item.expiresAt <= Date.now()) {
            this.values.delete(key);
            return undefined;
        }
        this.values.delete(key);
        this.values.set(key, item);
        return item.value;
    }

    set(key: string, value: T): void {
        this.values.delete(key);
        this.values.set(key, { value, expiresAt: Date.now() + this.ttlMs });
        while (this.values.size > this.maxEntries) {
            this.values.delete(this.values.keys().next().value as string);
        }
    }

    clear(): void {
        this.values.clear();
    }
}
