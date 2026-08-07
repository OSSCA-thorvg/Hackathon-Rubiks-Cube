import './style.css';
import { bootstrap } from './bootstrap.ts';

const app = document.querySelector<HTMLDivElement>('#app');
if (app === null) {
  throw new Error('Missing #app container.');
}

void bootstrap(app);
