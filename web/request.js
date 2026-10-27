// The firmware serves one HTTP client at a time. Hold the slot through the body,
// so telemetry cannot build a parallel socket queue during an off-channel scan.
const queue = [];
let busy = false, active = null;
function pump() {
  if (busy || !queue.length) return;
  const job = queue.shift();
  if (job.signal?.aborted) { job.reject(new DOMException("Aborted", "AbortError")); pump(); return; }
  busy = true;
  job.signal?.removeEventListener("abort", job.cancel);
  const controller = new AbortController();
  active = { method: job.options.method || "GET", cancel: () => controller.abort() };
  const cancel = () => controller.abort();
  job.signal?.addEventListener("abort", cancel, { once: true });
  const timer = setTimeout(cancel, 8000);
  (async () => {
    try {
      const { priority, ...options } = job.options;
      const response = await fetch(job.url, { ...options, signal: controller.signal });
      const bytes = await response.arrayBuffer();
      job.resolve(new Response([204,205,304].includes(response.status)?null:bytes, {
        status: response.status, statusText: response.statusText, headers: response.headers,
      }));
    } catch (error) { job.reject(error); }
    finally {
      clearTimeout(timer);
      job.signal?.removeEventListener("abort", cancel);
      busy = false; active = null;
      pump();
    }
  })();
}
export function deviceFetch(url, options = {}) {
  return new Promise((resolve, reject) => {
    const job = { url, options, signal: options.signal, resolve, reject };
    job.cancel = () => {
      const index = queue.indexOf(job);
      if (index >= 0) queue.splice(index, 1);
      reject(new DOMException("Aborted", "AbortError"));
    };
    if (job.signal?.aborted) { job.cancel(); return; }
    job.signal?.addEventListener("abort", job.cancel, { once: true });
    if (options.method === "POST") queue.unshift(job);
    else queue.push(job);
    if (options.priority && active?.method === "GET") active.cancel();
    pump();
  });
}
