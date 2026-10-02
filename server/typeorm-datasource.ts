import { loadConfig } from './config';
import { createDataSource } from './database';

export default createDataSource(loadConfig());
