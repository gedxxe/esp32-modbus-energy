import { useCallback, useEffect, useRef, useState } from "react";
import { catalog } from "./i18n.js";

export default function useDevice() {
  const [data, setData] = useState(null);
  const [fresh, setFresh] = useState(false);
  const [pending, setPending] = useState(false);
  const [notice, setNotice] = useState(null);
  const inFlight = useRef(false);
  const mounted = useRef(true);
  const getAbort = useRef(null);
  const poll = useCallback(async () => {
    if (inFlight.current) return;
    inFlight.current = true;
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
      if (!Array.isArray(next.slots) || next.slots.length !== 9 || !next.token)
        throw new Error("schema");
      if (mounted.current) {
        setData(next);
        setFresh(true);
      }
    } catch {
      if (mounted.current) setFresh(false);
    } finally {
      clearTimeout(timer);
      inFlight.current = false;
    }
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
  const act = async (path, fields = {}) => {
    if (pending || !fresh || !data) return;
    setPending(true);
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
      setNotice(
        catalog.en[body.code] ? { key: body.code } : { text: body.message },
      );
      if (response.ok) poll();
    } catch (error) {
      setNotice({
        key: error.name === "AbortError" ? "request_timeout" : "request_failed",
      });
    } finally {
      clearTimeout(timer);
      setPending(false);
    }
  };
  return { data, fresh, pending, notice, setNotice, poll, act };
}
