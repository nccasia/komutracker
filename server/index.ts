import { loadConfig } from './config';
import { start } from './app';

start(loadConfig()).catch((error: unknown) => {
    console.error(error instanceof Error ? error.message : error);
    process.exitCode = 1;
});
