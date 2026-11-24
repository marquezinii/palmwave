use std::time::{Duration, Instant};

/// Wall time between consecutive successful DXGI presents. Keep the last
/// timestamp across reports so a long stall belongs to exactly one window.
pub(super) struct PresentTiming {
    start: Instant,
    last: Instant,
    intervals: Vec<Duration>,
}

pub(super) struct Summary {
    pub intervals: usize,
    pub elapsed: Duration,
    pub p95: Duration,
    pub p99: Duration,
    pub max: Duration,
}

impl PresentTiming {
    pub(super) fn new(first_present: Instant) -> Self {
        Self {
            start: first_present,
            last: first_present,
            intervals: Vec::new(),
        }
    }

    pub(super) fn record(&mut self, now: Instant) -> Option<Summary> {
        self.intervals.push(now - self.last);
        self.last = now;
        let elapsed = now - self.start;
        if elapsed < Duration::from_secs(5) {
            return None;
        }

        self.intervals.sort_unstable();
        let count = self.intervals.len();
        let percentile = |p: usize| self.intervals[(count * p).div_ceil(100) - 1];
        let summary = Summary {
            intervals: count,
            elapsed,
            p95: percentile(95),
            p99: percentile(99),
            max: self.intervals[count - 1],
        };
        self.intervals.clear();
        self.start = now;
        Some(summary)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn measures_every_interval_and_keeps_the_slowest_frame() {
        let mut now = Instant::now();
        let mut timing = PresentTiming::new(now);
        for ms in [50; 4].into_iter().chain([32; 95]) {
            now += Duration::from_millis(ms);
            assert!(timing.record(now).is_none());
        }
        now += Duration::from_millis(1900);
        let window = timing.record(now).unwrap();
        assert_eq!(window.intervals, 100);
        assert_eq!(window.elapsed, Duration::from_millis(5140));
        assert_eq!(window.p95, Duration::from_millis(32));
        assert_eq!(window.p99, Duration::from_millis(50));
        assert_eq!(window.max, Duration::from_millis(1900));

        // No successful present for eight seconds after the report.
        let stall = Duration::from_secs(8);
        let window = timing.record(now + stall).unwrap();
        assert_eq!(window.intervals, 1);
        assert_eq!(window.elapsed, stall);
        assert_eq!((window.p95, window.p99, window.max), (stall, stall, stall));
    }
}
