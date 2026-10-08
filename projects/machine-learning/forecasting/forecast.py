"""An expanding-window, seven-day forecast. Every demand feature predates its origin."""

from datetime import date, timedelta
import numpy as np
from sklearn.linear_model import Ridge
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler

LAGS = (7, 14, 21, 28)


def features(counts, dates, indices, origin=None):
    counts = np.asarray(counts, float)
    result = []
    for index in indices:
        if index < max(LAGS) or index >= len(dates):
            raise ValueError("target lacks required history")
        if origin is not None and index - min(LAGS) >= origin:
            raise ValueError("feature crosses forecast origin")
        day = dates[index]
        year_phase = 2 * np.pi * (day.timetuple().tm_yday - 1) / 365.25
        calendar = [
            (day - dates[0]).days / 365.25,
            np.sin(year_phase),
            np.cos(year_phase),
        ]
        calendar += [float(day.weekday() == j) for j in range(7)]
        result.append([counts[index - lag] for lag in LAGS] + calendar)
    return np.asarray(result, dtype=np.float64)


def forecast_origin(counts, dates, origin, horizon=7, alpha=10.0):
    counts = np.asarray(counts, float)
    if not 1 <= horizon <= 7 or origin <= max(LAGS) or origin + horizon > len(counts):
        raise ValueError("valid history and one-to-seven-day horizon required")
    train = np.arange(max(LAGS), origin)
    target = np.arange(origin, origin + horizon)
    model = make_pipeline(StandardScaler(), Ridge(alpha=alpha)).fit(
        features(counts, dates, train), counts[train]
    )
    prediction = np.maximum(0, model.predict(features(counts, dates, target, origin)))
    baseline = counts[target - 7]
    seasonal_mean = np.mean([counts[target - lag] for lag in LAGS], axis=0)
    return prediction, baseline, seasonal_mean


def rolling(counts, dates, start, stop, alpha):
    reports = []
    for origin in range(start, stop, 7):
        horizon = min(7, stop - origin)
        prediction, baseline, average = forecast_origin(
            counts, dates, origin, horizon, alpha
        )
        for offset in range(horizon):
            i = origin + offset
            reports.append(
                {
                    "date": dates[i].isoformat(),
                    "origin": dates[origin].isoformat(),
                    "horizon_day": offset + 1,
                    "actual": float(counts[i]),
                    "ridge": float(prediction[offset]),
                    "last_week": float(baseline[offset]),
                    "four_week_mean": float(average[offset]),
                }
            )
    return reports


def metrics(rows):
    target = np.array([r["actual"] for r in rows])
    return {
        model: {
            "mae": float(np.mean(np.abs(np.array([r[model] for r in rows]) - target))),
            "rmse": float(
                np.sqrt(np.mean((np.array([r[model] for r in rows]) - target) ** 2))
            ),
        }
        for model in ["ridge", "last_week", "four_week_mean"]
    }
