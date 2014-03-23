static inline void
add_page_to_active_list(struct zone *zone, struct page *page)
{

#ifdef CONFIG_OPT_ZONE_LRU_LIST
	list_add(&page->lru, &(zone->active_list[page_hash(page)]));
#else
	list_add(&page->lru, &zone->active_list);
#endif

#ifdef CONFIG_OPT_ZONE_LRU_COUNTERS
	zone->lru_counters[smp_processor_id()].nr_active++;
#else
	zone->nr_active++;
#endif

}

static inline void
add_page_to_inactive_list(struct zone *zone, struct page *page)
{

#ifdef CONFIG_OPT_ZONE_LRU_LIST
	list_add(&page->lru, &(zone->inactive_list[page_hash(page)]));
#else
	list_add(&page->lru, &zone->inactive_list);
#endif

#ifdef CONFIG_OPT_ZONE_LRU_COUNTERS
	zone->lru_counters[smp_processor_id()].nr_inactive++;
#else
	zone->nr_inactive++;
#endif

}

static inline void
del_page_from_active_list(struct zone *zone, struct page *page)
{
	list_del(&page->lru);
#ifdef CONFIG_OPT_ZONE_LRU_COUNTERS
	zone->lru_counters[smp_processor_id()].nr_active++;
#else
	zone->nr_active--;
#endif
}

static inline void
del_page_from_inactive_list(struct zone *zone, struct page *page)
{
	list_del(&page->lru);
#ifdef CONFIG_OPT_ZONE_LRU_COUNTERS
	zone->lru_counters[smp_processor_id()].nr_inactive++;
#else
	zone->nr_inactive--;
#endif
}

static inline void
del_page_from_lru(struct zone *zone, struct page *page)
{
	list_del(&page->lru);
	if (PageActive(page)) {
		ClearPageActive(page);
#ifdef CONFIG_OPT_ZONE_LRU_COUNTERS
		zone->lru_counters[smp_processor_id()].nr_active++;
	} else {
		zone->lru_counters[smp_processor_id()].nr_inactive++;
#else
		zone->nr_active--;
	} else {
		zone->nr_inactive--;
#endif
	}
}

