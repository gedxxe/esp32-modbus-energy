import { useCallback, useEffect, useRef, useState } from "react";
import { catalog } from "./i18n.js";
import { validStatus } from "./validation.js";

const reached = (value, target) => ((value - target) >>> 0) < 0x80000000;
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

export default function useDevice() {
  const [data, setData] = useState(null);
  const [fresh, setFresh] = useState(false);
  const [pending, setPending] = useState(false);
  const [priorityPending, setPriorityPending] = useState(false);
  const [notice, setNotice] = useState(null);
  const inFlight = useRef(null);
  const actionInFlight = useRef(false);
  const priorityInFlight = useRef(false);
  const mounted = useRef(true);
  const getAbort = useRef(null);
  const poll = useCallback(() => {
    if (inFlight.current) return inFlight.current;
    const request = (async () => {
      const controller = new AbortController();
      getAbort.current = controller;
      const timer = setTimeout(() => controller.abort(), 4000);
      try {
        const response = await fetch("/api/status", {
          cache: "no-store",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error("status");
        const next = await response.json();
        if (!validStatus(next)) throw new Error("schema");
        if (mounted.current) {
          setData(next);
          setFresh(true);
        }
        return next;
      } catch {
        if (mounted.current) setFresh(false);
        return null;
      } finally {
        clearTimeout(timer);
        inFlight.current = null;
      }
    })();
    inFlight.current = request;
    return request;
  }, []);
  useEffect(() => {
    mounted.current = true;
    poll();
    const timer = setInterval(poll, 2500);
    return () => {
      mounted.current = false;
      clearInterval(timer);
      getAbort.current?.abort();
    };
  }, [poll]);
  const act = async (path, fields = {}, { priority = false } = {}) => {
    priority = priority && path === "/api/diagnostic" && ["x", "stop", "resume"].includes(fields.command);
    const lock = priority ? priorityInFlight : actionInFlight;
    // A ref closes the same-render window before React applies disabled controls.
    if (lock.current || !fresh || !data) return null;
    lock.current = true;
    (priority ? setPriorityPending : setPending)(true);
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 5000);
    try {
      const response = await fetch(path, {
        method: "POST",
        signal: controller.signal,
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: new URLSearchParams({ ...fields, token: data.token }),
      });
      const body = await response.json();
      if (mounted.current) setNotice(
        catalog.en[body.code] ? { key: body.code } : { text: body.message },
      );
      if (response.ok) {
        // A 202 acknowledges admission, not execution. Keep controls locked until
        // the UART owner publishes completion, even if an older GET was in flight.
        const commandId = body.commandId;
        const deadline = Date.now() + 40000;
        do {
          const next = await poll();
          if (!mounted.current) return null;
          if (next && next.token !== data.token) throw new Error("session");
          if (!Number.isInteger(commandId)) break;
          if (next?.control && reached(next.control.completed, commandId)) {
            const outcome = next.control.results?.find((row) => row.id === commandId);
            if (!outcome) {
              setNotice({ key: "request_timeout" });
              return { ok: false, body };
            }
            if (outcome.error) {
              setNotice(catalog.en[outcome.error] ? { key: outcome.error } : { text: outcome.error });
              return { ok: false, body };
            }
            return { ok: true, body };
          }
          await delay(250);
        } while (Date.now() < deadline);
        if (Number.isInteger(commandId)) {
          setNotice({ key: "request_timeout" });
          return { ok: false, body };
        }
      }
      return { ok: response.ok, body };
    } catch (error) {
      if (mounted.current) setNotice({
        key: error.name === "AbortError" ? "request_timeout" : "request_failed",
      });
      return null;
    } finally {
      clearTimeout(timer);
      lock.current = false;
      if (mounted.current) (priority ? setPriorityPending : setPending)(false);
    }
  };
  return { data, fresh, pending, priorityPending, notice, setNotice, poll, act };
}
