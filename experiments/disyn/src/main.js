import './styles.css';
import { AppView } from './ui/AppView.js';

const bootstrap = () => {
  const container = document.getElementById('app');
  if (!container) {
    throw new Error('[Disyn] #app container missing');
  }

  const app = new AppView(container);
  app.mount();
};

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', bootstrap);
} else {
  bootstrap();
}

