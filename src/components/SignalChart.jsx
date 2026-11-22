import React from 'react';
import { PpgPlot } from './PpgPlot.jsx';
export function SignalChart({
  processorRef,
  historySeconds,
  setHistorySeconds,
  active,
  measurements,
}) {
  return (
    <section className="panel chart-panel">
      <div className="panel-heading">
        <div>
          <p className="eyebrow">FORMA DE UNDĂ</p>
          <h2>Semnal PPG</h2>
        </div>
        <div className="segmented" aria-label="Istoric grafic">
          {[5, 10, 20].map((s) => (
            <button
              key={s}
              className={historySeconds === s ? 'selected' : ''}
              onClick={() => setHistorySeconds(s)}
            >
              {s} s
            </button>
          ))}
        </div>
      </div>
      <div className="chart-legend">
        <span>
          <i className="raw-line" />
          IR brut · centrat și inversat
        </span>
        <span>
          <i />
          IR filtrat
        </span>
        <span>
          <i className="beat-dot" />
          Bătaie detectată
        </span>
      </div>
      <div className="plot-wrap">
        <PpgPlot processorRef={processorRef} historySeconds={historySeconds} running={active} />
        {!measurements.count && (
          <div className="chart-empty">
            <span>∿</span>Semnalul tău începe aici
            <small>Conectează placa sau pornește simulatorul.</small>
          </div>
        )}
      </div>
      <div className="chart-footer">
        <span>
          Aceeași scară de amplitudine. Brutul este centrat și inversat doar pentru comparație;
          zgomotul nu este filtrat.
        </span>
        <span className="mono">100 eșantioane/s nominal</span>
      </div>
    </section>
  );
}
