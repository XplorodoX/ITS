"use client";

import { useEffect, useRef, useState } from "react";
import styles from "./admin.module.css";

// ── Types ──────────────────────────────────────────────────────────────────────

type QType = "mcq" | "estimate" | "higher_lower" | "poti_target" | "temp_target";
type AnyQ  = Record<string, unknown>;

interface SetSummary { name: string; count: number; active: boolean; }

const TYPE_LABELS: Record<QType, string> = {
  mcq:          "Multiple Choice",
  estimate:     "Schätzfrage",
  higher_lower: "Höher / Niedriger",
  poti_target:  "Poti-Challenge",
  temp_target:  "Temperatur-Challenge",
};

const DEFAULTS: Record<QType, AnyQ> = {
  mcq:          { type: "mcq",          text: "", options: { A: "", B: "", C: "", D: "" }, correct: "A", time_limit_s: 20 },
  estimate:     { type: "estimate",     text: "", min: 0, max: 100, unit: "", correct: 50, time_limit_s: 30 },
  higher_lower: { type: "higher_lower", text: "", reference: 100, unit: "", correct: "HIGHER", actual: 0, time_limit_s: 20 },
  poti_target:  { type: "poti_target",  text: "", target: 50, tolerance: 5, time_limit_s: 20 },
  temp_target:  { type: "temp_target",  text: "", target: 25.0, tolerance: 2.0, time_limit_s: 20 },
};

const API = process.env.NEXT_PUBLIC_API_URL ?? "http://localhost:8080";

// ── Question form fields ───────────────────────────────────────────────────────

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className={styles.field}>
      <label className={styles.fieldLabel}>{label}</label>
      {children}
    </div>
  );
}

function QuestionForm({ q, onChange }: { q: AnyQ; onChange: (q: AnyQ) => void }) {
  const type = (q.type ?? "mcq") as QType;
  const set  = (key: string, val: unknown) => onChange({ ...q, [key]: val });

  return (
    <div className={styles.qForm}>
      <Field label="Fragentext">
        <textarea
          className={styles.textarea}
          value={String(q.text ?? "")}
          rows={2}
          onChange={e => set("text", e.target.value)}
        />
      </Field>

      <Field label="Zeitlimit (s)">
        <input type="number" className={styles.input} min={5} max={300}
          value={Number(q.time_limit_s ?? 20)}
          onChange={e => set("time_limit_s", Number(e.target.value))} />
      </Field>

      {type === "mcq" && (
        <>
          {(["A", "B", "C", "D"] as const).map(k => (
            <Field key={k} label={`Option ${k}`}>
              <input className={styles.input}
                value={String((q.options as Record<string, string>)?.[k] ?? "")}
                onChange={e => set("options", { ...(q.options as object), [k]: e.target.value })} />
            </Field>
          ))}
          <Field label="Richtige Antwort">
            <select className={styles.select} value={String(q.correct ?? "A")}
              onChange={e => set("correct", e.target.value)}>
              {["A", "B", "C", "D"].map(k => <option key={k}>{k}</option>)}
            </select>
          </Field>
        </>
      )}

      {type === "estimate" && (
        <>
          <Field label="Min"><input type="number" className={styles.input} value={Number(q.min ?? 0)} onChange={e => set("min", Number(e.target.value))} /></Field>
          <Field label="Max"><input type="number" className={styles.input} value={Number(q.max ?? 100)} onChange={e => set("max", Number(e.target.value))} /></Field>
          <Field label="Einheit (optional)"><input className={styles.input} value={String(q.unit ?? "")} onChange={e => set("unit", e.target.value)} /></Field>
          <Field label="Richtige Antwort"><input type="number" className={styles.input} value={Number(q.correct ?? 0)} onChange={e => set("correct", Number(e.target.value))} /></Field>
        </>
      )}

      {type === "higher_lower" && (
        <>
          <Field label="Referenzwert"><input type="number" className={styles.input} value={Number(q.reference ?? 0)} onChange={e => set("reference", Number(e.target.value))} /></Field>
          <Field label="Tatsächlicher Wert"><input type="number" className={styles.input} value={Number(q.actual ?? 0)} onChange={e => set("actual", Number(e.target.value))} /></Field>
          <Field label="Einheit (optional)"><input className={styles.input} value={String(q.unit ?? "")} onChange={e => set("unit", e.target.value)} /></Field>
          <Field label="Richtige Antwort">
            <select className={styles.select} value={String(q.correct ?? "HIGHER")} onChange={e => set("correct", e.target.value)}>
              <option value="HIGHER">↑ Höher</option>
              <option value="LOWER">↓ Niedriger</option>
            </select>
          </Field>
        </>
      )}

      {(type === "poti_target" || type === "temp_target") && (
        <>
          <Field label={type === "temp_target" ? "Zieltemperatur (°C)" : "Zielwert (%)"}>
            <input type="number" className={styles.input} value={Number(q.target ?? 50)} onChange={e => set("target", Number(e.target.value))} />
          </Field>
          <Field label="Toleranz">
            <input type="number" className={styles.input} step="0.5" value={Number(q.tolerance ?? 5)} onChange={e => set("tolerance", Number(e.target.value))} />
          </Field>
        </>
      )}
    </div>
  );
}

// ── Main page ──────────────────────────────────────────────────────────────────

export default function AdminPage() {
  const [sets,        setSets]        = useState<SetSummary[]>([]);
  const [currentSet,  setCurrentSet]  = useState<string | null>(null);
  const [questions,   setQuestions]   = useState<AnyQ[]>([]);
  const [dirty,       setDirty]       = useState(false);
  const [expandedIdx, setExpandedIdx] = useState<number | null>(null);
  const [addType,     setAddType]     = useState<QType>("mcq");
  const [showAddForm, setShowAddForm] = useState(false);
  const [newQ,        setNewQ]        = useState<AnyQ>(DEFAULTS.mcq);
  const [newSetName,  setNewSetName]  = useState("");
  const [status,      setStatus]      = useState<{ msg: string; ok: boolean } | null>(null);
  const dragFrom = useRef<number | null>(null);

  useEffect(() => { loadSets(); }, []);

  function flash(msg: string, ok = true) {
    setStatus({ msg, ok });
    setTimeout(() => setStatus(null), 3000);
  }

  async function loadSets() {
    try {
      const res = await fetch(`${API}/api/question-sets`);
      setSets(await res.json());
    } catch { flash("API nicht erreichbar", false); }
  }

  async function openSet(name: string) {
    try {
      const res = await fetch(`${API}/api/question-sets/${name}`);
      if (!res.ok) { flash(`Set '${name}' nicht gefunden`, false); return; }
      setQuestions(await res.json());
      setCurrentSet(name);
      setDirty(false);
      setExpandedIdx(null);
      setShowAddForm(false);
    } catch { flash("Ladefehler", false); }
  }

  async function saveSet() {
    if (!currentSet) return;
    try {
      const res = await fetch(`${API}/api/question-sets/${currentSet}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(questions),
      });
      const data = await res.json();
      if (!res.ok) { flash(data.errors?.[0] ?? data.error ?? "Fehler", false); return; }
      setDirty(false);
      flash(`Gespeichert (${data.count} Fragen)`);
      loadSets();
    } catch { flash("Speicherfehler", false); }
  }

  async function deleteSet(name: string) {
    if (!confirm(`Set '${name}' löschen?`)) return;
    try {
      const res = await fetch(`${API}/api/question-sets/${name}`, { method: "DELETE" });
      const data = await res.json();
      if (!res.ok) { flash(data.error ?? "Fehler", false); return; }
      if (currentSet === name) { setCurrentSet(null); setQuestions([]); }
      flash(`'${name}' gelöscht`);
      loadSets();
    } catch { flash("Fehler", false); }
  }

  async function createSet() {
    const name = newSetName.trim();
    if (!name || !/^[A-Za-z0-9_-]+$/.test(name)) {
      flash("Name: nur Buchstaben, Zahlen, - und _", false); return;
    }
    try {
      const res = await fetch(`${API}/api/question-sets/${name}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify([]),
      });
      if (!res.ok) { flash("Konnte Set nicht erstellen", false); return; }
      setNewSetName("");
      flash(`Set '${name}' erstellt`);
      loadSets();
      openSet(name);
    } catch { flash("Fehler", false); }
  }

  async function setActive(name: string) {
    try {
      const res = await fetch(`${API}/api/active-set`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name }),
      });
      const data = await res.json();
      if (!res.ok) { flash(data.error ?? "Fehler", false); return; }
      flash(`'${name}' ist jetzt aktiv`);
      loadSets();
    } catch { flash("Fehler", false); }
  }

  // ── Question operations ──────────────────────────────────────────────────────

  function updateQ(idx: number, q: AnyQ) {
    setQuestions(prev => prev.map((old, i) => i === idx ? q : old));
    setDirty(true);
  }

  function deleteQ(idx: number) {
    setQuestions(prev => prev.filter((_, i) => i !== idx));
    if (expandedIdx === idx) setExpandedIdx(null);
    setDirty(true);
  }

  function addQuestion() {
    setQuestions(prev => [...prev, { ...newQ }]);
    setExpandedIdx(questions.length); // expand the new one
    setShowAddForm(false);
    setNewQ(DEFAULTS[addType]);
    setDirty(true);
  }

  function handleTypeChange(t: QType) {
    setAddType(t);
    setNewQ(DEFAULTS[t]);
  }

  // ── Drag & drop reorder ──────────────────────────────────────────────────────

  function onDragStart(idx: number) { dragFrom.current = idx; }

  function onDragOver(e: React.DragEvent, idx: number) {
    e.preventDefault();
    e.dataTransfer.dropEffect = "move";
  }

  function onDrop(idx: number) {
    const from = dragFrom.current;
    if (from === null || from === idx) return;
    dragFrom.current = null;
    setQuestions(prev => {
      const next = [...prev];
      const [moved] = next.splice(from, 1);
      next.splice(idx, 0, moved);
      return next;
    });
    if (expandedIdx === from) setExpandedIdx(idx);
    setDirty(true);
  }

  // ── Render ───────────────────────────────────────────────────────────────────

  return (
    <div className={styles.page}>
      <header className={styles.header}>
        <h1 className={styles.title}>Quiz Admin</h1>
        <span className={styles.apiUrl}>{API}</span>
        {status && (
          <span className={`${styles.status} ${status.ok ? styles.statusOk : styles.statusErr}`}>
            {status.msg}
          </span>
        )}
      </header>

      <div className={styles.layout}>
        {/* ── Sidebar: set list ── */}
        <aside className={styles.sidebar}>
          <h2 className={styles.sidebarTitle}>Fragen-Sets</h2>
          <ul className={styles.setList}>
            {sets.map(s => (
              <li key={s.name}
                className={`${styles.setItem} ${currentSet === s.name ? styles.setItemActive : ""}`}
              >
                <button className={styles.setName} onClick={() => openSet(s.name)}>
                  {s.active && <span className={styles.activeDot} title="Aktives Set" />}
                  {s.name}
                  <span className={styles.setCount}>{s.count}</span>
                </button>
                <div className={styles.setActions}>
                  {!s.active && (
                    <button className={styles.btnActivate} onClick={() => setActive(s.name)} title="Als aktives Set setzen">
                      ▶
                    </button>
                  )}
                  <button className={styles.btnDelete} onClick={() => deleteSet(s.name)} title="Set löschen">
                    ✕
                  </button>
                </div>
              </li>
            ))}
          </ul>

          <div className={styles.newSetForm}>
            <input
              className={styles.input}
              placeholder="neues-set"
              value={newSetName}
              onChange={e => setNewSetName(e.target.value)}
              onKeyDown={e => e.key === "Enter" && createSet()}
            />
            <button className={styles.btnCreate} onClick={createSet}>+ Set</button>
          </div>
        </aside>

        {/* ── Editor ── */}
        <main className={styles.editor}>
          {!currentSet ? (
            <p className={styles.placeholder}>← Set auswählen oder neu erstellen</p>
          ) : (
            <>
              <div className={styles.editorHeader}>
                <span className={styles.editorTitle}>{currentSet}.json</span>
                <div className={styles.editorActions}>
                  {dirty && (
                    <button className={styles.btnDiscard} onClick={() => { openSet(currentSet); }}>
                      Verwerfen
                    </button>
                  )}
                  <button className={styles.btnSave} onClick={saveSet} disabled={!dirty}>
                    Speichern{dirty ? " *" : ""}
                  </button>
                </div>
              </div>

              {/* Question list */}
              <ol className={styles.qList}>
                {questions.map((q, idx) => (
                  <li
                    key={idx}
                    className={styles.qRow}
                    draggable
                    onDragStart={() => onDragStart(idx)}
                    onDragOver={e => onDragOver(e, idx)}
                    onDrop={() => onDrop(idx)}
                  >
                    <span className={styles.dragHandle} title="Ziehen zum Sortieren">⠿</span>
                    <span className={styles.qNum}>{idx + 1}</span>
                    <span className={styles.qType}>{TYPE_LABELS[(q.type ?? "mcq") as QType]}</span>
                    <span className={styles.qText}>{String(q.text || "—")}</span>
                    <button
                      className={styles.btnEdit}
                      onClick={() => setExpandedIdx(expandedIdx === idx ? null : idx)}
                    >
                      {expandedIdx === idx ? "▲" : "▼"}
                    </button>
                    <button className={styles.btnDeleteQ} onClick={() => deleteQ(idx)}>✕</button>

                    {expandedIdx === idx && (
                      <div className={styles.qFormWrapper}>
                        <QuestionForm q={q} onChange={updated => updateQ(idx, updated)} />
                      </div>
                    )}
                  </li>
                ))}
              </ol>

              {/* Add question */}
              {showAddForm ? (
                <div className={styles.addPanel}>
                  <div className={styles.addPanelHeader}>
                    <select className={styles.select} value={addType}
                      onChange={e => handleTypeChange(e.target.value as QType)}>
                      {(Object.keys(TYPE_LABELS) as QType[]).map(t => (
                        <option key={t} value={t}>{TYPE_LABELS[t]}</option>
                      ))}
                    </select>
                    <button className={styles.btnCancel} onClick={() => setShowAddForm(false)}>Abbrechen</button>
                  </div>
                  <QuestionForm q={newQ} onChange={setNewQ} />
                  <button className={styles.btnAdd} onClick={addQuestion}>+ Hinzufügen</button>
                </div>
              ) : (
                <button className={styles.btnShowAdd} onClick={() => setShowAddForm(true)}>
                  + Frage hinzufügen
                </button>
              )}
            </>
          )}
        </main>
      </div>
    </div>
  );
}
