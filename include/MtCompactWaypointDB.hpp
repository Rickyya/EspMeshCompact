#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "MtCompactStructs.hpp"

/**
 * @brief Handles an in-memory database of received waypoints.
 *
 * Waypoints are keyed by their Meshtastic id, so a re-broadcast of a waypoint
 * that is already known updates it in place rather than accumulating copies.
 *
 * Expiry is deliberately not enforced here. This component has no wall clock,
 * and MCT_Waypoint::expire is a unix timestamp, so deciding whether an entry is
 * stale is left to the application, which does have one. purge() is provided
 * for callers that want to act on that decision.
 */
class WaypointDB {
   public:
    static constexpr size_t MAX_WAYPOINTS = 16;

    class iterator {
       public:
        iterator(MCT_Waypoint* waypoints, bool* valid, size_t idx)
            : waypoints_(waypoints), valid_(valid), idx_(idx) {
            advance_to_valid();
        }
        iterator& operator++() {
            ++idx_;
            advance_to_valid();
            return *this;
        }
        MCT_Waypoint& operator*() { return waypoints_[idx_]; }
        MCT_Waypoint* operator->() { return &waypoints_[idx_]; }
        bool operator!=(const iterator& other) const { return idx_ != other.idx_; }
        bool operator==(const iterator& other) const { return idx_ == other.idx_; }

       private:
        void advance_to_valid() {
            while (idx_ < MAX_WAYPOINTS && !valid_[idx_]) {
                ++idx_;
            }
        }
        MCT_Waypoint* waypoints_;
        bool* valid_;
        size_t idx_;
    };

    iterator begin() { return iterator(waypoints, valid, 0); }
    iterator end() { return iterator(waypoints, valid, MAX_WAYPOINTS); }

    /**
     * @brief Insert a waypoint, or update the stored copy if its id is known.
     *
     * @return true if this id was not previously stored.
     */
    bool upsert(const MCT_Waypoint& waypoint) {
        size_t free_slot = MAX_WAYPOINTS;
        for (size_t i = 0; i < MAX_WAYPOINTS; ++i) {
            if (valid[i]) {
                if (waypoints[i].id == waypoint.id) {
                    waypoints[i] = waypoint;
                    return false;
                }
            } else if (free_slot == MAX_WAYPOINTS) {
                free_slot = i;
            }
        }
        // Full: overwrite the oldest entry, matching how NodeInfoDB behaves.
        if (free_slot == MAX_WAYPOINTS) {
            free_slot = oldest;
            oldest = (oldest + 1) % MAX_WAYPOINTS;
        }
        waypoints[free_slot] = waypoint;
        valid[free_slot] = true;
        return true;
    }

    MCT_Waypoint* get(uint32_t id) {
        for (size_t i = 0; i < MAX_WAYPOINTS; ++i) {
            if (valid[i] && waypoints[i].id == id) {
                return &waypoints[i];
            }
        }
        return nullptr;
    }

    bool remove(uint32_t id) {
        for (size_t i = 0; i < MAX_WAYPOINTS; ++i) {
            if (valid[i] && waypoints[i].id == id) {
                valid[i] = false;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Drop every entry whose expire is non-zero and at or before now.
     *
     * @param now_unix Current unix time, supplied by the application.
     * @return how many entries were dropped.
     */
    size_t purge(uint32_t now_unix) {
        size_t dropped = 0;
        for (size_t i = 0; i < MAX_WAYPOINTS; ++i) {
            if (valid[i] && waypoints[i].expire != 0 && waypoints[i].expire <= now_unix) {
                valid[i] = false;
                ++dropped;
            }
        }
        return dropped;
    }

    size_t count() const {
        size_t n = 0;
        for (size_t i = 0; i < MAX_WAYPOINTS; ++i) {
            if (valid[i]) ++n;
        }
        return n;
    }

    void clear() {
        memset(valid, 0, sizeof(valid));
        oldest = 0;
    }

   private:
    MCT_Waypoint waypoints[MAX_WAYPOINTS] = {};
    bool valid[MAX_WAYPOINTS] = {};
    size_t oldest = 0;
};
