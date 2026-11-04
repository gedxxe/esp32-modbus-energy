// SPDX-License-Identifier: GPL-3.0-only
// Closed details must not suppress submission without a visible explanation.
export function validateFormFields(form, t, report) {
  const invalid = Array.from(form.elements).find(
    (field) => field.willValidate && !field.checkValidity(),
  );
  if (!invalid) return true;
  const field = invalid.labels?.[0]?.textContent ||
    invalid.getAttribute("aria-label") || t("network_title");
  report(invalid.validity.valueMissing ? "network_required_field" : "network_invalid_field", { field });
  for (let parent = invalid.parentElement; parent; parent = parent.parentElement) {
    if (parent.tagName === "DETAILS") parent.open = true;
  }
  requestAnimationFrame(() => {
    if (!invalid.isConnected) return;
    invalid.focus({ preventScroll: true });
    invalid.scrollIntoView({ block: "center", behavior:
      matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth" });
  });
  return false;
}
