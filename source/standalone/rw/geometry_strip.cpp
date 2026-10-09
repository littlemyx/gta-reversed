// P2B-04b follow-up: the exe's tristrip generator, ported 1:1 (see geometry_strip.h). Naming of the exe's structures:
//   TriNode (0x24 bytes, freelist of 0x75A090): +0 index of the triangle in the mesh, +4/+8/+0xC the three Edge nodes (edge k = v[k] -> v[k+1]),
//     +0x10/+0x14 next/prev in its bucket list, +0x18 "used", +0x1C scratch "used" (trial strips), +0x20 (byte) number of neighbours.
//   Edge (0x10 bytes, 0x75A1B0): +0/+2 the two vertices, +4 first triangle, +8 second triangle (0 until a triangle with the reversed edge shows up).
//   buckets: four doubly linked lists of the not yet used triangles by number of unused neighbours (0..3); new strips start from the
//   first triangle of the lowest non-empty bucket (isolated triangles are emitted on their own first).
// NOTSA deviations (no effect on the output): the exe scans a linked list of ALL edges for an unmatched reversed edge (0x75A1B0); the shim keeps
// the same candidates in a hash of per-(a,b) stacks, newest first, which is the first hit of that scan. Freelists / mallocs are plain vectors.
#ifdef NOTSA_RW_LIBRW
#include "geometry_strip.h"

#include <list>
#include <unordered_map>
#include <deque>

namespace RwShim {
namespace {
struct Edge;
struct TriNode {
    uint32_t  idx{};
    Edge*     e[3]{};
    TriNode*  next{};
    TriNode*  prev{};
    int       used{};
    int       scratch{};
    uint8_t   deg{};
};
struct Edge {
    uint16_t a{}, b{};
    TriNode* t1{};
    TriNode* t2{};
};
using Strip = std::vector<uint16_t>;

struct Stripper {
    const uint16_t (*tris)[3]{};
    size_t           n{};
    std::vector<TriNode> nodes;
    std::deque<Edge>     edges;
    std::unordered_map<uint32_t, std::vector<Edge*>> open; // (a<<16|b) -> edges without second triangle, newest last
    TriNode* bucket[256]{};
    std::list<Strip> strips; // newest first (the exe prepends)

    // The exe's flag test used everywhere: a triangle is available when it exists and its (scratch) used flag is 0. mode < 4 = trial strip.
    static bool Avail(const TriNode* t, int mode) {
        return t && (mode < 4 ? t->scratch : t->used) == 0;
    }

    // 0x75A1B0
    Edge* AddEdge(uint16_t va, uint16_t vb, uint32_t triIdx) {
        auto it = open.find((uint32_t(vb) << 16) | va); // stored[1] == va && stored[0] == vb
        if (it != open.end() && !it->second.empty()) {
            Edge* e = it->second.back();
            it->second.pop_back();
            e->t1->deg++;
            nodes[triIdx].deg++;
            e->t2 = &nodes[triIdx];
            return e;
        }
        Edge& e = edges.emplace_back();
        e.a  = va;
        e.b  = vb;
        e.t1 = &nodes[triIdx];
        open[(uint32_t(va) << 16) | vb].push_back(&e);
        return &e;
    }

    // 0x75A090 + the bucket fill at the top of 0x759790
    void Build() {
        nodes.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            TriNode& t = nodes[i];
            t.idx = i;
            const uint16_t* v = tris[i];
            t.e[0] = AddEdge(v[0], v[1], i);
            t.e[1] = AddEdge(v[1], v[2], i);
            t.e[2] = AddEdge(v[2], v[0], i);
        }
        for (uint32_t i = 0; i < n; i++) {
            TriNode* t = &nodes[i];
            t->next = bucket[t->deg];
            if (t->next) {
                t->next->prev = t;
            }
            bucket[t->deg] = t;
            t->prev = nullptr;
        }
    }

    void Unlink(TriNode* p) {
        if (bucket[p->deg] == p) {
            bucket[p->deg] = p->next;
            if (bucket[p->deg]) {
                bucket[p->deg]->prev = nullptr;
            }
        } else {
            if (p->next) {
                p->next->prev = p->prev;
            }
            if (p->prev) {
                p->prev->next = p->next;
            }
        }
    }

    // 0x75A250: trial strips only mark the scratch flag; a real strip marks the triangle used, takes it out of its bucket and moves every
    // unused neighbour one bucket down.
    void Take(TriNode* p, int mode) {
        if (mode < 4) {
            p->scratch = 1;
            return;
        }
        p->used = 1;
        Unlink(p);
        for (int k = 0; k < 3; k++) {
            const Edge* e = p->e[k];
            TriNode* q = e->t1;
            if (!(q && q != p && q->used == 0)) {
                q = e->t2;
                if (!(q && q->used == 0)) {
                    continue;
                }
            }
            Unlink(q);
            q->deg--;
            q->next = bucket[q->deg];
            if (q->next) {
                q->next->prev = q;
            }
            bucket[q->deg] = q;
            q->prev = nullptr;
        }
    }

    // 0x75A370: grows strip `s` over edge `e` as far as possible; returns the number of triangles taken.
    int Extend(Strip& s, Edge* e, int mode) {
        int   count = 0;
        Edge* carry = nullptr; // local_14: survives iterations of this call
        if (!e) {
            return 0;
        }
        for (;;) {
            TriNode* nb = e->t1;
            if (!Avail(nb, mode)) {
                nb = e->t2;
                if (!Avail(nb, mode)) {
                    return count;
                }
            }
            count++;
            Take(nb, mode);
            int pos = -1;
            if (nb->e[0] == e) {
                pos = 0;
            } else if (nb->e[1] == e) {
                pos = 1;
            } else if (nb->e[2] == e) {
                pos = 2;
            }
            const Edge* e2 = nb->e[(pos + 1) % 3];
            const uint16_t nv   = (e2->t1 == nb) ? e2->b : e2->a; // vertex of nb opposite to e
            const size_t   cnt  = s.size();
            const uint16_t last = s[cnt - 1];
            Edge* nx = nullptr; // edge of nb between the strip's last vertex and the new one
            for (int k = 0; k < 3 && !nx; k++) {
                Edge* c = nb->e[k];
                if ((c->a == last && c->b == nv) || (c->a == nv && c->b == last)) {
                    nx = c;
                }
            }
            if (nx && (Avail(nx->t1, mode) || Avail(nx->t2, mode))) {
                s.push_back(nv); // straight on
                e = nx;
                continue;
            }
            // cannot continue straight: remember the first edge of nb that is neither e nor nx
            if (!(nb->e[0] == e || nb->e[0] == nx)) {
                carry = nb->e[0];
            } else if (!(nb->e[1] == e || nb->e[1] == nx)) {
                carry = nb->e[1];
            } else if (nb->e[2] != e && nb->e[2] != nx) {
                carry = nb->e[2];
            }
            if (!carry || !(Avail(carry->t1, mode) || Avail(carry->t2, mode)) || (cnt & 1)) {
                s.push_back(nv);
                return count;
            }
            s.push_back(s[cnt - 2]); // degenerate turn
            s.push_back(nv);
            e = carry;
        }
    }

    // 0x759790 main loop
    void Run(bool tryAll) {
        Build();
        size_t done = 0;
        while (done < n) {
            if (TriNode* t = bucket[0]) { // isolated triangle
                strips.push_front(Strip{tris[t->idx][0], tris[t->idx][1], tris[t->idx][2]});
                t->used = 1;
                t->scratch = 1;
                bucket[0] = t->next;
                if (bucket[0]) {
                    bucket[0]->prev = nullptr;
                }
                done++;
                continue;
            }
            int k = 1;
            while (k < 4 && !bucket[k]) {
                k++;
            }
            if (k == 4) {
                break; // NOTSA: the exe would spin here; unreachable for consistent bucket state
            }
            TriNode* t = bucket[k];
            auto n2 = [&](const Edge* e) { return int(Avail(e->t1, 4)) + int(Avail(e->t2, 4)); };
            const Edge *E0 = t->e[0], *E1 = t->e[1], *E2 = t->e[2];
            // first guess of the start rotation: prefer a vertex whose two edges both continue (>= 2 available triangles incl. t itself)
            unsigned start;
            if (n2(E2) > 1 && n2(E1) > 1) {
                start = 1;
            } else if (n2(E0) > 1 && n2(E2) > 1) {
                start = 2;
            } else if (n2(E1) > 1 && n2(E0) > 1) {
                start = 0;
            } else if (n2(E1) < n2(E0)) {
                start = (n2(E2) < n2(E0)) + 1;
            } else {
                start = (n2(E1) <= n2(E2)) ? 1 : 0;
            }
            unsigned bestRot = start;
            unsigned bestCost = 0;
            int mode = tryAll ? 0 : 3;
            for (;;) {
                if (mode < 3) {
                    for (auto& x : nodes) {
                        x.scratch = x.used;
                    }
                }
                const int m = mode + 1; // 4 = the real pass
                unsigned rot = (mode == 0) ? start % 3 : (mode == 1) ? (start + 1) % 3 : (mode == 2) ? (start + 2) % 3 : bestRot;
                Edge *fwd = nullptr, *back = nullptr;
                if (rot == 0) {
                    fwd = t->e[1]; back = t->e[0];
                } else if (rot == 1) {
                    fwd = t->e[2]; back = t->e[1];
                } else {
                    fwd = t->e[0]; back = t->e[2];
                }
                const uint16_t* v = tris[t->idx];
                Strip a{v[rot % 3], v[(rot + 1) % 3], v[(rot + 2) % 3]};
                Take(t, m);
                unsigned cost = unsigned(Extend(a, fwd, m)) + 1 + unsigned(done);
                if (!(Avail(back->t1, m) || Avail(back->t2, m))) {
                    if (bestCost < cost) {
                        bestCost = cost;
                        bestRot = rot;
                    }
                    if (m > 3) {
                        strips.push_front(std::move(a));
                        done = cost;
                        break;
                    }
                    mode = m;
                    continue;
                }
                Strip b{a[1], a[0]};
                cost += unsigned(Extend(b, back, m));
                if (b.size() & 1) {
                    b.push_back(b[b.size() - 2]);
                }
                if (bestCost < cost) {
                    bestCost = cost;
                    bestRot = rot;
                }
                if (m >= 4) {
                    Strip s;
                    s.reserve(a.size() + b.size());
                    for (size_t i = b.size(); i > 2; i--) {
                        s.push_back(b[i - 1]);
                    }
                    s.insert(s.end(), a.begin(), a.end());
                    strips.push_front(std::move(s));
                    done = cost;
                    break;
                }
                mode = m;
            }
        }
    }
};

// 0x75A660: joins all strips (newest first) into one index list
void Join(std::list<Strip>& l, bool pad, std::vector<uint16_t>& r) {
    if (l.empty()) {
        return;
    }
    r = std::move(l.front());
    l.pop_front();
    while (!l.empty()) {
        const size_t c = r.size();
        auto pick = l.begin();
        auto it = l.begin();
        for (; it != l.end(); ++it) {
            if ((*it)[0] == r[c - 1]) {
                break;
            }
        }
        if (it != l.end()) {
            pick = it;
            if ((c & 1) && pad) {
                r.push_back((*it)[0]);
            }
        } else {
            for (it = l.begin(); it != l.end(); ++it) {
                if ((*it)[0] == r[c - 2]) {
                    break;
                }
            }
            if (it != l.end()) {
                pick = it;
                r.push_back((*it)[0]);
                if (!((c & 1) || !pad)) {
                    r.push_back((*it)[0]);
                }
            } else {
                pick = l.begin();
                r.push_back(r[c - 1]);
                r.push_back((*pick)[0]);
                if ((c & 1) && pad) {
                    r.push_back((*pick)[0]);
                }
            }
        }
        r.insert(r.end(), pick->begin(), pick->end());
        l.erase(pick);
    }
}
} // namespace

void TriStripMesh(const uint16_t (*tris)[3], size_t numTris, bool tryAllStarts, bool padParity, std::vector<uint16_t>& out) {
    Stripper s;
    s.tris = tris;
    s.n    = numTris;
    s.Run(tryAllStarts);
    Join(s.strips, padParity, out);
}
} // namespace RwShim
#endif
