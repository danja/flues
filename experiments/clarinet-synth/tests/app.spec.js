import { test, expect } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.goto('http://localhost:5173');
});

test('should have the correct title', async ({ page }) => {
  await expect(page).toHaveTitle('Digital Waveguide Clarinet Synthesizer');
});

test('should turn on the synth when power button is clicked', async ({ page }) => {
  await page.click('#power-button');
  const status = await page.textContent('#status');
  expect(status).toBe('ON');
});

test('should change the note when a key is clicked', async ({ page }) => {
  await page.click('#power-button');
  await page.click('[data-note="C4"]');
  const note = await page.textContent('#current-note');
  expect(note).toBe('C4');
});
