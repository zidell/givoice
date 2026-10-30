use std::collections::HashMap;
use std::hash::Hash;

// Model responses may arrive for many historical API keys during a long app
// lifetime. Retain a small cache and drop old keys/results on admission.
pub fn insert_bounded<K: Eq + Hash + Clone, V>(
    cache: &mut HashMap<K, V>,
    key: K,
    value: V,
    limit: usize,
) {
    if limit == 0 {
        cache.clear();
        return;
    }
    if !cache.contains_key(&key) && cache.len() >= limit {
        if let Some(old) = cache.keys().next().cloned() {
            cache.remove(&old);
        }
    }
    cache.insert(key, value);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{atomic::{AtomicUsize, Ordering}, Arc};

    struct Value(Arc<AtomicUsize>);
    impl Drop for Value {
        fn drop(&mut self) { self.0.fetch_sub(1, Ordering::SeqCst); }
    }
    #[test]
    fn historical_entries_are_dropped() {
        let live = Arc::new(AtomicUsize::new(0));
        let mut cache = HashMap::new();
        for i in 0..10000 {
            live.fetch_add(1, Ordering::SeqCst);
            insert_bounded(&mut cache, i.to_string(), Value(live.clone()), 8);
            assert!(cache.len() <= 8);
            assert_eq!(live.load(Ordering::SeqCst), cache.len());
        }
        drop(cache);
        assert_eq!(live.load(Ordering::SeqCst), 0);
    }
    #[test]
    fn updating_an_entry_does_not_evict_another() {
        let mut cache = HashMap::from([(1, 1), (2, 2)]);
        insert_bounded(&mut cache, 1, 3, 2);
        assert_eq!(cache, HashMap::from([(1, 3), (2, 2)]));
    }
}
