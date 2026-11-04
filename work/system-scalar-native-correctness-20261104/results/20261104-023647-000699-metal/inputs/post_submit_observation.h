#pragma once

#include <cstdint>
#include <vector>

// Test-driver observation only: no source is issued, rebound or authorized here.
namespace SystemScalarPostSubmit {
template <class Fact>
bool native_row(const Fact &fact) {
    return fact.semantic_kind == 4 || fact.semantic_kind == 5;
}

template <class Fact>
bool exact_source(const Fact &a, const Fact &b) {
    return a.native_serial == b.native_serial && a.completion_serial == b.completion_serial &&
            a.backend_queue == b.backend_queue && a.semantic_kind == b.semantic_kind && a.command_slot == b.command_slot;
}

template <class Fact>
bool named_source(const Fact &fact) {
    return native_row(fact) && fact.native_serial && fact.completion_serial && fact.backend_queue &&
            ((fact.semantic_kind == 4 && fact.command_slot == UINT32_MAX) ||
                    (fact.semantic_kind == 5 && fact.command_slot != UINT32_MAX));
}

template <class Fact>
bool pending_source(const Fact &fact) {
    return named_source(fact) && fact.issue == 2 && fact.completion == 1 && !fact.error && !fact.native_code;
}

// Pending is not completion. Every possible native source must be represented
// by a real original row, and every observed source keeps its exact identity
// until that same row refines through the original completion callback.
template <class Report, class Fact>
bool observe_pending(const Report &report, const std::vector<Fact> &facts, std::vector<Fact> &pending) {
    if (report.native_error || report.native_code ||
            (report.state != 2 && report.state != 3 && report.state != 4 && report.state != 8)) {
        return false;
    }
    uint32_t possible = 0;
    auto next = pending;
    for (uint32_t i = 0; i < facts.size(); ++i) {
        const auto &fact = facts[i];
        if (fact.error || fact.native_code) {
            return false;
        }
        if (!native_row(fact)) {
            continue;
        }
        if (!named_source(fact)) {
            return false;
        }
        // Native serials are unique within the original driver. Duplicate or
        // reassociated rows cannot explain separate possible obligations.
        for (uint32_t j = 0; j < i; ++j) {
            if (native_row(facts[j]) && facts[j].native_serial == fact.native_serial) {
                return false;
            }
        }
        if (fact.issue == 2) {
            if (!pending_source(fact)) {
                return false;
            }
            ++possible;
            bool seen = false;
            for (const auto &old : next) {
                if (old.native_serial == fact.native_serial) {
                    if (!exact_source(old, fact)) {
                        return false;
                    }
                    seen = true;
                }
            }
            if (!seen) {
                next.push_back(fact);
            }
        } else if (fact.issue != 1 || (fact.completion != 1 && fact.completion != 2)) {
            return false;
        }
    }
    if (possible != report.possible_count || (possible && report.state == 8)) {
        return false;
    }
    for (const auto &old : next) {
        bool found = false;
        for (const auto &fact : facts) {
            if (!native_row(fact) || old.native_serial != fact.native_serial) {
                continue;
            }
            if (!exact_source(old, fact) || (!pending_source(fact) &&
                    !(fact.issue == 1 && fact.completion == 2 && !fact.error && !fact.native_code))) {
                return false;
            }
            found = true;
        }
        if (!found) {
            return false;
        }
    }
    pending.swap(next);
    return true;
}

// A hidden preparation submission uses UINT32_MAX and cannot stand in for
// submission of this System dispatch. Match the sealed Body's actual native
// projection against a real row from this same original ticket/fact revision.
template <class Report, class Fact>
bool issued_body(const Report &report, const std::vector<Fact> &facts, Fact &body_out, Fact &native_out, uint32_t &native_index) {
    if (report.possible_count || report.native_error || report.native_code ||
            (report.state != 2 && report.state != 3 && report.state != 4 && report.state != 8)) {
        return false;
    }
    const Fact *body = nullptr;
    for (const auto &fact : facts) {
        if (fact.semantic_kind == 2) {
            if (body) {
                return false;
            }
            body = &fact;
        }
    }
    if (!body || !body->graph_instance || body->command_slot == UINT32_MAX ||
            !body->native_serial || !body->completion_serial || !body->backend_queue || body->error || body->native_code) {
        return false;
    }
    for (uint32_t i = 0; i < facts.size(); ++i) {
        const auto &fact = facts[i];
        if (named_source(fact) && fact.command_slot == body->command_slot && fact.native_serial == body->native_serial &&
                fact.completion_serial == body->completion_serial && fact.backend_queue == body->backend_queue &&
                fact.issue == 1 && (fact.completion == 1 || fact.completion == 2) && !fact.error && !fact.native_code) {
            body_out = *body;
            native_out = fact;
            native_index = i;
            return true;
        }
    }
    return false;
}

// Terminal success of some other row is not return of the selected original
// source. Extra later readback/native rows are allowed; old identities are not.
template <class Fact>
bool original_sources_returned(const std::vector<Fact> &facts, const std::vector<Fact> &pending,
        const Fact &issued_body, const Fact &issued_native) {
    auto returned = [&](const Fact &old) {
        uint32_t count = 0;
        for (const auto &fact : facts) {
            if (native_row(fact) && fact.native_serial == old.native_serial) {
                if (!named_source(fact) || !exact_source(old, fact) || fact.issue != 1 || fact.completion != 2 || fact.error || fact.native_code) {
                    return false;
                }
                ++count;
            }
        }
        return count == 1;
    };
    if (!returned(issued_native)) {
        return false;
    }
    for (const auto &old : pending) {
        if (!returned(old)) {
            return false;
        }
    }
    uint32_t bodies = 0;
    for (const auto &fact : facts) {
        if (fact.semantic_kind != 2) {
            continue;
        }
        ++bodies;
        if (fact.graph_instance != issued_body.graph_instance || fact.graph_frame != issued_body.graph_frame ||
                fact.graph_kind != issued_body.graph_kind || fact.graph_index != issued_body.graph_index ||
                fact.graph_offset != issued_body.graph_offset || fact.command_slot != issued_body.command_slot ||
                fact.native_serial != issued_body.native_serial || fact.completion_serial != issued_body.completion_serial ||
                fact.backend_queue != issued_body.backend_queue || fact.issue != 1 || fact.completion != 2 || fact.error || fact.native_code) {
            return false;
        }
    }
    return bodies == 1;
}
} // namespace SystemScalarPostSubmit
