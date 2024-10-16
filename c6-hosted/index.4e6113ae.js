class $ec3de3bad43fb783$var$A extends Error {
}
/*! pako 2.1.0 https://github.com/nodeca/pako @license (MIT AND Zlib) */ function $ec3de3bad43fb783$var$t(A) {
    let t = A.length;
    for(; --t >= 0;)A[t] = 0;
}
const $ec3de3bad43fb783$var$e = 256, $ec3de3bad43fb783$var$i = 286, $ec3de3bad43fb783$var$s = 30, $ec3de3bad43fb783$var$a = 15, $ec3de3bad43fb783$var$n = new Uint8Array([
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    5,
    5,
    5,
    5,
    0
]), $ec3de3bad43fb783$var$E = new Uint8Array([
    0,
    0,
    0,
    0,
    1,
    1,
    2,
    2,
    3,
    3,
    4,
    4,
    5,
    5,
    6,
    6,
    7,
    7,
    8,
    8,
    9,
    9,
    10,
    10,
    11,
    11,
    12,
    12,
    13,
    13
]), $ec3de3bad43fb783$var$h = new Uint8Array([
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    3,
    7
]), $ec3de3bad43fb783$var$r = new Uint8Array([
    16,
    17,
    18,
    0,
    8,
    7,
    9,
    6,
    10,
    5,
    11,
    4,
    12,
    3,
    13,
    2,
    14,
    1,
    15
]), $ec3de3bad43fb783$var$g = new Array(576);
$ec3de3bad43fb783$var$t($ec3de3bad43fb783$var$g);
const $ec3de3bad43fb783$var$o = new Array(60);
$ec3de3bad43fb783$var$t($ec3de3bad43fb783$var$o);
const $ec3de3bad43fb783$var$B = new Array(512);
$ec3de3bad43fb783$var$t($ec3de3bad43fb783$var$B);
const $ec3de3bad43fb783$var$w = new Array(256);
$ec3de3bad43fb783$var$t($ec3de3bad43fb783$var$w);
const $ec3de3bad43fb783$var$c = new Array(29);
$ec3de3bad43fb783$var$t($ec3de3bad43fb783$var$c);
const $ec3de3bad43fb783$var$C = new Array($ec3de3bad43fb783$var$s);
function $ec3de3bad43fb783$var$I(A, t, e, i, s) {
    this.static_tree = A, this.extra_bits = t, this.extra_base = e, this.elems = i, this.max_length = s, this.has_stree = A && A.length;
}
let $ec3de3bad43fb783$var$_, $ec3de3bad43fb783$var$l, $ec3de3bad43fb783$var$d;
function $ec3de3bad43fb783$var$M(A, t) {
    this.dyn_tree = A, this.max_code = 0, this.stat_desc = t;
}
$ec3de3bad43fb783$var$t($ec3de3bad43fb783$var$C);
const $ec3de3bad43fb783$var$D = (A)=>A < 256 ? $ec3de3bad43fb783$var$B[A] : $ec3de3bad43fb783$var$B[256 + (A >>> 7)], $ec3de3bad43fb783$var$R = (A, t)=>{
    A.pending_buf[A.pending++] = 255 & t, A.pending_buf[A.pending++] = t >>> 8 & 255;
}, $ec3de3bad43fb783$var$S = (A, t, e)=>{
    A.bi_valid > 16 - e ? (A.bi_buf |= t << A.bi_valid & 65535, $ec3de3bad43fb783$var$R(A, A.bi_buf), A.bi_buf = t >> 16 - A.bi_valid, A.bi_valid += e - 16) : (A.bi_buf |= t << A.bi_valid & 65535, A.bi_valid += e);
}, $ec3de3bad43fb783$var$Q = (A, t, e)=>{
    $ec3de3bad43fb783$var$S(A, e[2 * t], e[2 * t + 1]);
}, $ec3de3bad43fb783$var$f = (A, t)=>{
    let e = 0;
    do e |= 1 & A, A >>>= 1, e <<= 1;
    while (--t > 0);
    return e >>> 1;
}, $ec3de3bad43fb783$var$F = (A, t, e)=>{
    const i = new Array(16);
    let s, n, E = 0;
    for(s = 1; s <= $ec3de3bad43fb783$var$a; s++)E = E + e[s - 1] << 1, i[s] = E;
    for(n = 0; n <= t; n++){
        let t = A[2 * n + 1];
        0 !== t && (A[2 * n] = $ec3de3bad43fb783$var$f(i[t]++, t));
    }
}, $ec3de3bad43fb783$var$T = (A)=>{
    let t;
    for(t = 0; t < $ec3de3bad43fb783$var$i; t++)A.dyn_ltree[2 * t] = 0;
    for(t = 0; t < $ec3de3bad43fb783$var$s; t++)A.dyn_dtree[2 * t] = 0;
    for(t = 0; t < 19; t++)A.bl_tree[2 * t] = 0;
    A.dyn_ltree[512] = 1, A.opt_len = A.static_len = 0, A.sym_next = A.matches = 0;
}, $ec3de3bad43fb783$var$u = (A)=>{
    A.bi_valid > 8 ? $ec3de3bad43fb783$var$R(A, A.bi_buf) : A.bi_valid > 0 && (A.pending_buf[A.pending++] = A.bi_buf), A.bi_buf = 0, A.bi_valid = 0;
}, $ec3de3bad43fb783$var$p = (A, t, e, i)=>{
    const s = 2 * t, a = 2 * e;
    return A[s] < A[a] || A[s] === A[a] && i[t] <= i[e];
}, $ec3de3bad43fb783$var$y = (A, t, e)=>{
    const i = A.heap[e];
    let s = e << 1;
    for(; s <= A.heap_len && (s < A.heap_len && $ec3de3bad43fb783$var$p(t, A.heap[s + 1], A.heap[s], A.depth) && s++, !$ec3de3bad43fb783$var$p(t, i, A.heap[s], A.depth));)A.heap[e] = A.heap[s], e = s, s <<= 1;
    A.heap[e] = i;
}, $ec3de3bad43fb783$var$k = (A, t, i)=>{
    let s, a, h, r, g = 0;
    if (0 !== A.sym_next) do s = 255 & A.pending_buf[A.sym_buf + g++], s += (255 & A.pending_buf[A.sym_buf + g++]) << 8, a = A.pending_buf[A.sym_buf + g++], 0 === s ? $ec3de3bad43fb783$var$Q(A, a, t) : (h = $ec3de3bad43fb783$var$w[a], $ec3de3bad43fb783$var$Q(A, h + $ec3de3bad43fb783$var$e + 1, t), r = $ec3de3bad43fb783$var$n[h], 0 !== r && (a -= $ec3de3bad43fb783$var$c[h], $ec3de3bad43fb783$var$S(A, a, r)), s--, h = $ec3de3bad43fb783$var$D(s), $ec3de3bad43fb783$var$Q(A, h, i), r = $ec3de3bad43fb783$var$E[h], 0 !== r && (s -= $ec3de3bad43fb783$var$C[h], $ec3de3bad43fb783$var$S(A, s, r)));
    while (g < A.sym_next);
    $ec3de3bad43fb783$var$Q(A, 256, t);
}, $ec3de3bad43fb783$var$H = (A, t)=>{
    const e = t.dyn_tree, i = t.stat_desc.static_tree, s = t.stat_desc.has_stree, n = t.stat_desc.elems;
    let E, h, r, g = -1;
    for(A.heap_len = 0, A.heap_max = 573, E = 0; E < n; E++)0 !== e[2 * E] ? (A.heap[++A.heap_len] = g = E, A.depth[E] = 0) : e[2 * E + 1] = 0;
    for(; A.heap_len < 2;)r = A.heap[++A.heap_len] = g < 2 ? ++g : 0, e[2 * r] = 1, A.depth[r] = 0, A.opt_len--, s && (A.static_len -= i[2 * r + 1]);
    for(t.max_code = g, E = A.heap_len >> 1; E >= 1; E--)$ec3de3bad43fb783$var$y(A, e, E);
    r = n;
    do E = A.heap[1], A.heap[1] = A.heap[A.heap_len--], $ec3de3bad43fb783$var$y(A, e, 1), h = A.heap[1], A.heap[--A.heap_max] = E, A.heap[--A.heap_max] = h, e[2 * r] = e[2 * E] + e[2 * h], A.depth[r] = (A.depth[E] >= A.depth[h] ? A.depth[E] : A.depth[h]) + 1, e[2 * E + 1] = e[2 * h + 1] = r, A.heap[1] = r++, $ec3de3bad43fb783$var$y(A, e, 1);
    while (A.heap_len >= 2);
    A.heap[--A.heap_max] = A.heap[1], ((A, t)=>{
        const e = t.dyn_tree, i = t.max_code, s = t.stat_desc.static_tree, n = t.stat_desc.has_stree, E = t.stat_desc.extra_bits, h = t.stat_desc.extra_base, r = t.stat_desc.max_length;
        let g, o, B, w, c, C, I = 0;
        for(w = 0; w <= $ec3de3bad43fb783$var$a; w++)A.bl_count[w] = 0;
        for(e[2 * A.heap[A.heap_max] + 1] = 0, g = A.heap_max + 1; g < 573; g++)o = A.heap[g], w = e[2 * e[2 * o + 1] + 1] + 1, w > r && (w = r, I++), e[2 * o + 1] = w, o > i || (A.bl_count[w]++, c = 0, o >= h && (c = E[o - h]), C = e[2 * o], A.opt_len += C * (w + c), n && (A.static_len += C * (s[2 * o + 1] + c)));
        if (0 !== I) {
            do {
                for(w = r - 1; 0 === A.bl_count[w];)w--;
                A.bl_count[w]--, A.bl_count[w + 1] += 2, A.bl_count[r]--, I -= 2;
            }while (I > 0);
            for(w = r; 0 !== w; w--)for(o = A.bl_count[w]; 0 !== o;)B = A.heap[--g], B > i || (e[2 * B + 1] !== w && (A.opt_len += (w - e[2 * B + 1]) * e[2 * B], e[2 * B + 1] = w), o--);
        }
    })(A, t), $ec3de3bad43fb783$var$F(e, g, A.bl_count);
}, $ec3de3bad43fb783$var$P = (A, t, e)=>{
    let i, s, a = -1, n = t[1], E = 0, h = 7, r = 4;
    for(0 === n && (h = 138, r = 3), t[2 * (e + 1) + 1] = 65535, i = 0; i <= e; i++)s = n, n = t[2 * (i + 1) + 1], ++E < h && s === n || (E < r ? A.bl_tree[2 * s] += E : 0 !== s ? (s !== a && A.bl_tree[2 * s]++, A.bl_tree[32]++) : E <= 10 ? A.bl_tree[34]++ : A.bl_tree[36]++, E = 0, a = s, 0 === n ? (h = 138, r = 3) : s === n ? (h = 6, r = 3) : (h = 7, r = 4));
}, $ec3de3bad43fb783$var$O = (A, t, e)=>{
    let i, s, a = -1, n = t[1], E = 0, h = 7, r = 4;
    for(0 === n && (h = 138, r = 3), i = 0; i <= e; i++)if (s = n, n = t[2 * (i + 1) + 1], !(++E < h && s === n)) {
        if (E < r) do $ec3de3bad43fb783$var$Q(A, s, A.bl_tree);
        while (0 != --E);
        else 0 !== s ? (s !== a && ($ec3de3bad43fb783$var$Q(A, s, A.bl_tree), E--), $ec3de3bad43fb783$var$Q(A, 16, A.bl_tree), $ec3de3bad43fb783$var$S(A, E - 3, 2)) : E <= 10 ? ($ec3de3bad43fb783$var$Q(A, 17, A.bl_tree), $ec3de3bad43fb783$var$S(A, E - 3, 3)) : ($ec3de3bad43fb783$var$Q(A, 18, A.bl_tree), $ec3de3bad43fb783$var$S(A, E - 11, 7));
        E = 0, a = s, 0 === n ? (h = 138, r = 3) : s === n ? (h = 6, r = 3) : (h = 7, r = 4);
    }
};
let $ec3de3bad43fb783$var$U = !1;
const $ec3de3bad43fb783$var$G = (A, t, e, i)=>{
    $ec3de3bad43fb783$var$S(A, 0 + (i ? 1 : 0), 3), $ec3de3bad43fb783$var$u(A), $ec3de3bad43fb783$var$R(A, e), $ec3de3bad43fb783$var$R(A, ~e), e && A.pending_buf.set(A.window.subarray(t, t + e), A.pending), A.pending += e;
};
var $ec3de3bad43fb783$var$m = (A, t, i, s)=>{
    let a, n, E = 0;
    A.level > 0 ? (2 === A.strm.data_type && (A.strm.data_type = ((A)=>{
        let t, i = 4093624447;
        for(t = 0; t <= 31; t++, i >>>= 1)if (1 & i && 0 !== A.dyn_ltree[2 * t]) return 0;
        if (0 !== A.dyn_ltree[18] || 0 !== A.dyn_ltree[20] || 0 !== A.dyn_ltree[26]) return 1;
        for(t = 32; t < $ec3de3bad43fb783$var$e; t++)if (0 !== A.dyn_ltree[2 * t]) return 1;
        return 0;
    })(A)), $ec3de3bad43fb783$var$H(A, A.l_desc), $ec3de3bad43fb783$var$H(A, A.d_desc), E = ((A)=>{
        let t;
        for($ec3de3bad43fb783$var$P(A, A.dyn_ltree, A.l_desc.max_code), $ec3de3bad43fb783$var$P(A, A.dyn_dtree, A.d_desc.max_code), $ec3de3bad43fb783$var$H(A, A.bl_desc), t = 18; t >= 3 && 0 === A.bl_tree[2 * $ec3de3bad43fb783$var$r[t] + 1]; t--);
        return A.opt_len += 3 * (t + 1) + 5 + 5 + 4, t;
    })(A), a = A.opt_len + 3 + 7 >>> 3, n = A.static_len + 3 + 7 >>> 3, n <= a && (a = n)) : a = n = i + 5, i + 4 <= a && -1 !== t ? $ec3de3bad43fb783$var$G(A, t, i, s) : 4 === A.strategy || n === a ? ($ec3de3bad43fb783$var$S(A, 2 + (s ? 1 : 0), 3), $ec3de3bad43fb783$var$k(A, $ec3de3bad43fb783$var$g, $ec3de3bad43fb783$var$o)) : ($ec3de3bad43fb783$var$S(A, 4 + (s ? 1 : 0), 3), ((A, t, e, i)=>{
        let s;
        for($ec3de3bad43fb783$var$S(A, t - 257, 5), $ec3de3bad43fb783$var$S(A, e - 1, 5), $ec3de3bad43fb783$var$S(A, i - 4, 4), s = 0; s < i; s++)$ec3de3bad43fb783$var$S(A, A.bl_tree[2 * $ec3de3bad43fb783$var$r[s] + 1], 3);
        $ec3de3bad43fb783$var$O(A, A.dyn_ltree, t - 1), $ec3de3bad43fb783$var$O(A, A.dyn_dtree, e - 1);
    })(A, A.l_desc.max_code + 1, A.d_desc.max_code + 1, E + 1), $ec3de3bad43fb783$var$k(A, A.dyn_ltree, A.dyn_dtree)), $ec3de3bad43fb783$var$T(A), s && $ec3de3bad43fb783$var$u(A);
}, $ec3de3bad43fb783$var$Y = {
    _tr_init: (A)=>{
        $ec3de3bad43fb783$var$U || ((()=>{
            let A, t, e, r, M;
            const D = new Array(16);
            for(e = 0, r = 0; r < 28; r++)for($ec3de3bad43fb783$var$c[r] = e, A = 0; A < 1 << $ec3de3bad43fb783$var$n[r]; A++)$ec3de3bad43fb783$var$w[e++] = r;
            for($ec3de3bad43fb783$var$w[e - 1] = r, M = 0, r = 0; r < 16; r++)for($ec3de3bad43fb783$var$C[r] = M, A = 0; A < 1 << $ec3de3bad43fb783$var$E[r]; A++)$ec3de3bad43fb783$var$B[M++] = r;
            for(M >>= 7; r < $ec3de3bad43fb783$var$s; r++)for($ec3de3bad43fb783$var$C[r] = M << 7, A = 0; A < 1 << $ec3de3bad43fb783$var$E[r] - 7; A++)$ec3de3bad43fb783$var$B[256 + M++] = r;
            for(t = 0; t <= $ec3de3bad43fb783$var$a; t++)D[t] = 0;
            for(A = 0; A <= 143;)$ec3de3bad43fb783$var$g[2 * A + 1] = 8, A++, D[8]++;
            for(; A <= 255;)$ec3de3bad43fb783$var$g[2 * A + 1] = 9, A++, D[9]++;
            for(; A <= 279;)$ec3de3bad43fb783$var$g[2 * A + 1] = 7, A++, D[7]++;
            for(; A <= 287;)$ec3de3bad43fb783$var$g[2 * A + 1] = 8, A++, D[8]++;
            for($ec3de3bad43fb783$var$F($ec3de3bad43fb783$var$g, 287, D), A = 0; A < $ec3de3bad43fb783$var$s; A++)$ec3de3bad43fb783$var$o[2 * A + 1] = 5, $ec3de3bad43fb783$var$o[2 * A] = $ec3de3bad43fb783$var$f(A, 5);
            $ec3de3bad43fb783$var$_ = new $ec3de3bad43fb783$var$I($ec3de3bad43fb783$var$g, $ec3de3bad43fb783$var$n, 257, $ec3de3bad43fb783$var$i, $ec3de3bad43fb783$var$a), $ec3de3bad43fb783$var$l = new $ec3de3bad43fb783$var$I($ec3de3bad43fb783$var$o, $ec3de3bad43fb783$var$E, 0, $ec3de3bad43fb783$var$s, $ec3de3bad43fb783$var$a), $ec3de3bad43fb783$var$d = new $ec3de3bad43fb783$var$I(new Array(0), $ec3de3bad43fb783$var$h, 0, 19, 7);
        })(), $ec3de3bad43fb783$var$U = !0), A.l_desc = new $ec3de3bad43fb783$var$M(A.dyn_ltree, $ec3de3bad43fb783$var$_), A.d_desc = new $ec3de3bad43fb783$var$M(A.dyn_dtree, $ec3de3bad43fb783$var$l), A.bl_desc = new $ec3de3bad43fb783$var$M(A.bl_tree, $ec3de3bad43fb783$var$d), A.bi_buf = 0, A.bi_valid = 0, $ec3de3bad43fb783$var$T(A);
    },
    _tr_stored_block: $ec3de3bad43fb783$var$G,
    _tr_flush_block: $ec3de3bad43fb783$var$m,
    _tr_tally: (A, t, i)=>(A.pending_buf[A.sym_buf + A.sym_next++] = t, A.pending_buf[A.sym_buf + A.sym_next++] = t >> 8, A.pending_buf[A.sym_buf + A.sym_next++] = i, 0 === t ? A.dyn_ltree[2 * i]++ : (A.matches++, t--, A.dyn_ltree[2 * ($ec3de3bad43fb783$var$w[i] + $ec3de3bad43fb783$var$e + 1)]++, A.dyn_dtree[2 * $ec3de3bad43fb783$var$D(t)]++), A.sym_next === A.sym_end),
    _tr_align: (A)=>{
        $ec3de3bad43fb783$var$S(A, 2, 3), $ec3de3bad43fb783$var$Q(A, 256, $ec3de3bad43fb783$var$g), ((A)=>{
            16 === A.bi_valid ? ($ec3de3bad43fb783$var$R(A, A.bi_buf), A.bi_buf = 0, A.bi_valid = 0) : A.bi_valid >= 8 && (A.pending_buf[A.pending++] = 255 & A.bi_buf, A.bi_buf >>= 8, A.bi_valid -= 8);
        })(A);
    }
};
var $ec3de3bad43fb783$var$b = (A, t, e, i)=>{
    let s = 65535 & A | 0, a = A >>> 16 & 65535 | 0, n = 0;
    for(; 0 !== e;){
        n = e > 2e3 ? 2e3 : e, e -= n;
        do s = s + t[i++] | 0, a = a + s | 0;
        while (--n);
        s %= 65521, a %= 65521;
    }
    return s | a << 16 | 0;
};
const $ec3de3bad43fb783$var$K = new Uint32Array((()=>{
    let A, t = [];
    for(var e = 0; e < 256; e++){
        A = e;
        for(var i = 0; i < 8; i++)A = 1 & A ? 3988292384 ^ A >>> 1 : A >>> 1;
        t[e] = A;
    }
    return t;
})());
var $ec3de3bad43fb783$var$x = (A, t, e, i)=>{
    const s = $ec3de3bad43fb783$var$K, a = i + e;
    A ^= -1;
    for(let e = i; e < a; e++)A = A >>> 8 ^ s[255 & (A ^ t[e])];
    return -1 ^ A;
}, $ec3de3bad43fb783$var$L = {
    2: "need dictionary",
    1: "stream end",
    0: "",
    "-1": "file error",
    "-2": "stream error",
    "-3": "data error",
    "-4": "insufficient memory",
    "-5": "buffer error",
    "-6": "incompatible version"
}, $ec3de3bad43fb783$var$J = {
    Z_NO_FLUSH: 0,
    Z_PARTIAL_FLUSH: 1,
    Z_SYNC_FLUSH: 2,
    Z_FULL_FLUSH: 3,
    Z_FINISH: 4,
    Z_BLOCK: 5,
    Z_TREES: 6,
    Z_OK: 0,
    Z_STREAM_END: 1,
    Z_NEED_DICT: 2,
    Z_ERRNO: -1,
    Z_STREAM_ERROR: -2,
    Z_DATA_ERROR: -3,
    Z_MEM_ERROR: -4,
    Z_BUF_ERROR: -5,
    Z_NO_COMPRESSION: 0,
    Z_BEST_SPEED: 1,
    Z_BEST_COMPRESSION: 9,
    Z_DEFAULT_COMPRESSION: -1,
    Z_FILTERED: 1,
    Z_HUFFMAN_ONLY: 2,
    Z_RLE: 3,
    Z_FIXED: 4,
    Z_DEFAULT_STRATEGY: 0,
    Z_BINARY: 0,
    Z_TEXT: 1,
    Z_UNKNOWN: 2,
    Z_DEFLATED: 8
};
const { _tr_init: $ec3de3bad43fb783$var$z, _tr_stored_block: $ec3de3bad43fb783$var$N, _tr_flush_block: $ec3de3bad43fb783$var$v, _tr_tally: $ec3de3bad43fb783$var$j, _tr_align: $ec3de3bad43fb783$var$Z } = $ec3de3bad43fb783$var$Y, { Z_NO_FLUSH: $ec3de3bad43fb783$var$W, Z_PARTIAL_FLUSH: $ec3de3bad43fb783$var$X, Z_FULL_FLUSH: $ec3de3bad43fb783$var$q, Z_FINISH: $ec3de3bad43fb783$var$V, Z_BLOCK: $ec3de3bad43fb783$var$$, Z_OK: $ec3de3bad43fb783$var$AA, Z_STREAM_END: $ec3de3bad43fb783$var$tA, Z_STREAM_ERROR: $ec3de3bad43fb783$var$eA, Z_DATA_ERROR: $ec3de3bad43fb783$var$iA, Z_BUF_ERROR: $ec3de3bad43fb783$var$sA, Z_DEFAULT_COMPRESSION: $ec3de3bad43fb783$var$aA, Z_FILTERED: $ec3de3bad43fb783$var$nA, Z_HUFFMAN_ONLY: $ec3de3bad43fb783$var$EA, Z_RLE: $ec3de3bad43fb783$var$hA, Z_FIXED: $ec3de3bad43fb783$var$rA, Z_DEFAULT_STRATEGY: $ec3de3bad43fb783$var$gA, Z_UNKNOWN: $ec3de3bad43fb783$var$oA, Z_DEFLATED: $ec3de3bad43fb783$var$BA } = $ec3de3bad43fb783$var$J, $ec3de3bad43fb783$var$wA = 258, $ec3de3bad43fb783$var$cA = 262, $ec3de3bad43fb783$var$CA = 42, $ec3de3bad43fb783$var$IA = 113, $ec3de3bad43fb783$var$_A = 666, $ec3de3bad43fb783$var$lA = (A, t)=>(A.msg = $ec3de3bad43fb783$var$L[t], t), $ec3de3bad43fb783$var$dA = (A)=>2 * A - (A > 4 ? 9 : 0), $ec3de3bad43fb783$var$MA = (A)=>{
    let t = A.length;
    for(; --t >= 0;)A[t] = 0;
}, $ec3de3bad43fb783$var$DA = (A)=>{
    let t, e, i, s = A.w_size;
    t = A.hash_size, i = t;
    do e = A.head[--i], A.head[i] = e >= s ? e - s : 0;
    while (--t);
    t = s, i = t;
    do e = A.prev[--i], A.prev[i] = e >= s ? e - s : 0;
    while (--t);
};
let $ec3de3bad43fb783$var$RA = (A, t, e)=>(t << A.hash_shift ^ e) & A.hash_mask;
const $ec3de3bad43fb783$var$SA = (A)=>{
    const t = A.state;
    let e = t.pending;
    e > A.avail_out && (e = A.avail_out), 0 !== e && (A.output.set(t.pending_buf.subarray(t.pending_out, t.pending_out + e), A.next_out), A.next_out += e, t.pending_out += e, A.total_out += e, A.avail_out -= e, t.pending -= e, 0 === t.pending && (t.pending_out = 0));
}, $ec3de3bad43fb783$var$QA = (A, t)=>{
    $ec3de3bad43fb783$var$v(A, A.block_start >= 0 ? A.block_start : -1, A.strstart - A.block_start, t), A.block_start = A.strstart, $ec3de3bad43fb783$var$SA(A.strm);
}, $ec3de3bad43fb783$var$fA = (A, t)=>{
    A.pending_buf[A.pending++] = t;
}, $ec3de3bad43fb783$var$FA = (A, t)=>{
    A.pending_buf[A.pending++] = t >>> 8 & 255, A.pending_buf[A.pending++] = 255 & t;
}, $ec3de3bad43fb783$var$TA = (A, t, e, i)=>{
    let s = A.avail_in;
    return s > i && (s = i), 0 === s ? 0 : (A.avail_in -= s, t.set(A.input.subarray(A.next_in, A.next_in + s), e), 1 === A.state.wrap ? A.adler = $ec3de3bad43fb783$var$b(A.adler, t, s, e) : 2 === A.state.wrap && (A.adler = $ec3de3bad43fb783$var$x(A.adler, t, s, e)), A.next_in += s, A.total_in += s, s);
}, $ec3de3bad43fb783$var$uA = (A, t)=>{
    let e, i, s = A.max_chain_length, a = A.strstart, n = A.prev_length, E = A.nice_match;
    const h = A.strstart > A.w_size - $ec3de3bad43fb783$var$cA ? A.strstart - (A.w_size - $ec3de3bad43fb783$var$cA) : 0, r = A.window, g = A.w_mask, o = A.prev, B = A.strstart + $ec3de3bad43fb783$var$wA;
    let w = r[a + n - 1], c = r[a + n];
    A.prev_length >= A.good_match && (s >>= 2), E > A.lookahead && (E = A.lookahead);
    do if (e = t, r[e + n] === c && r[e + n - 1] === w && r[e] === r[a] && r[++e] === r[a + 1]) {
        a += 2, e++;
        do ;
        while (r[++a] === r[++e] && r[++a] === r[++e] && r[++a] === r[++e] && r[++a] === r[++e] && r[++a] === r[++e] && r[++a] === r[++e] && r[++a] === r[++e] && r[++a] === r[++e] && a < B);
        if (i = $ec3de3bad43fb783$var$wA - (B - a), a = B - $ec3de3bad43fb783$var$wA, i > n) {
            if (A.match_start = t, n = i, i >= E) break;
            w = r[a + n - 1], c = r[a + n];
        }
    }
    while ((t = o[t & g]) > h && 0 != --s);
    return n <= A.lookahead ? n : A.lookahead;
}, $ec3de3bad43fb783$var$pA = (A)=>{
    const t = A.w_size;
    let e, i, s;
    do {
        if (i = A.window_size - A.lookahead - A.strstart, A.strstart >= t + (t - $ec3de3bad43fb783$var$cA) && (A.window.set(A.window.subarray(t, t + t - i), 0), A.match_start -= t, A.strstart -= t, A.block_start -= t, A.insert > A.strstart && (A.insert = A.strstart), $ec3de3bad43fb783$var$DA(A), i += t), 0 === A.strm.avail_in) break;
        if (e = $ec3de3bad43fb783$var$TA(A.strm, A.window, A.strstart + A.lookahead, i), A.lookahead += e, A.lookahead + A.insert >= 3) for(s = A.strstart - A.insert, A.ins_h = A.window[s], A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[s + 1]); A.insert && (A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[s + 3 - 1]), A.prev[s & A.w_mask] = A.head[A.ins_h], A.head[A.ins_h] = s, s++, A.insert--, !(A.lookahead + A.insert < 3)););
    }while (A.lookahead < $ec3de3bad43fb783$var$cA && 0 !== A.strm.avail_in);
}, $ec3de3bad43fb783$var$yA = (A, t)=>{
    let e, i, s, a = A.pending_buf_size - 5 > A.w_size ? A.w_size : A.pending_buf_size - 5, n = 0, E = A.strm.avail_in;
    do {
        if (e = 65535, s = A.bi_valid + 42 >> 3, A.strm.avail_out < s) break;
        if (s = A.strm.avail_out - s, i = A.strstart - A.block_start, e > i + A.strm.avail_in && (e = i + A.strm.avail_in), e > s && (e = s), e < a && (0 === e && t !== $ec3de3bad43fb783$var$V || t === $ec3de3bad43fb783$var$W || e !== i + A.strm.avail_in)) break;
        n = t === $ec3de3bad43fb783$var$V && e === i + A.strm.avail_in ? 1 : 0, $ec3de3bad43fb783$var$N(A, 0, 0, n), A.pending_buf[A.pending - 4] = e, A.pending_buf[A.pending - 3] = e >> 8, A.pending_buf[A.pending - 2] = ~e, A.pending_buf[A.pending - 1] = ~e >> 8, $ec3de3bad43fb783$var$SA(A.strm), i && (i > e && (i = e), A.strm.output.set(A.window.subarray(A.block_start, A.block_start + i), A.strm.next_out), A.strm.next_out += i, A.strm.avail_out -= i, A.strm.total_out += i, A.block_start += i, e -= i), e && ($ec3de3bad43fb783$var$TA(A.strm, A.strm.output, A.strm.next_out, e), A.strm.next_out += e, A.strm.avail_out -= e, A.strm.total_out += e);
    }while (0 === n);
    return E -= A.strm.avail_in, E && (E >= A.w_size ? (A.matches = 2, A.window.set(A.strm.input.subarray(A.strm.next_in - A.w_size, A.strm.next_in), 0), A.strstart = A.w_size, A.insert = A.strstart) : (A.window_size - A.strstart <= E && (A.strstart -= A.w_size, A.window.set(A.window.subarray(A.w_size, A.w_size + A.strstart), 0), A.matches < 2 && A.matches++, A.insert > A.strstart && (A.insert = A.strstart)), A.window.set(A.strm.input.subarray(A.strm.next_in - E, A.strm.next_in), A.strstart), A.strstart += E, A.insert += E > A.w_size - A.insert ? A.w_size - A.insert : E), A.block_start = A.strstart), A.high_water < A.strstart && (A.high_water = A.strstart), n ? 4 : t !== $ec3de3bad43fb783$var$W && t !== $ec3de3bad43fb783$var$V && 0 === A.strm.avail_in && A.strstart === A.block_start ? 2 : (s = A.window_size - A.strstart, A.strm.avail_in > s && A.block_start >= A.w_size && (A.block_start -= A.w_size, A.strstart -= A.w_size, A.window.set(A.window.subarray(A.w_size, A.w_size + A.strstart), 0), A.matches < 2 && A.matches++, s += A.w_size, A.insert > A.strstart && (A.insert = A.strstart)), s > A.strm.avail_in && (s = A.strm.avail_in), s && ($ec3de3bad43fb783$var$TA(A.strm, A.window, A.strstart, s), A.strstart += s, A.insert += s > A.w_size - A.insert ? A.w_size - A.insert : s), A.high_water < A.strstart && (A.high_water = A.strstart), s = A.bi_valid + 42 >> 3, s = A.pending_buf_size - s > 65535 ? 65535 : A.pending_buf_size - s, a = s > A.w_size ? A.w_size : s, i = A.strstart - A.block_start, (i >= a || (i || t === $ec3de3bad43fb783$var$V) && t !== $ec3de3bad43fb783$var$W && 0 === A.strm.avail_in && i <= s) && (e = i > s ? s : i, n = t === $ec3de3bad43fb783$var$V && 0 === A.strm.avail_in && e === i ? 1 : 0, $ec3de3bad43fb783$var$N(A, A.block_start, e, n), A.block_start += e, $ec3de3bad43fb783$var$SA(A.strm)), n ? 3 : 1);
}, $ec3de3bad43fb783$var$kA = (A, t)=>{
    let e, i;
    for(;;){
        if (A.lookahead < $ec3de3bad43fb783$var$cA) {
            if ($ec3de3bad43fb783$var$pA(A), A.lookahead < $ec3de3bad43fb783$var$cA && t === $ec3de3bad43fb783$var$W) return 1;
            if (0 === A.lookahead) break;
        }
        if (e = 0, A.lookahead >= 3 && (A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[A.strstart + 3 - 1]), e = A.prev[A.strstart & A.w_mask] = A.head[A.ins_h], A.head[A.ins_h] = A.strstart), 0 !== e && A.strstart - e <= A.w_size - $ec3de3bad43fb783$var$cA && (A.match_length = $ec3de3bad43fb783$var$uA(A, e)), A.match_length >= 3) {
            if (i = $ec3de3bad43fb783$var$j(A, A.strstart - A.match_start, A.match_length - 3), A.lookahead -= A.match_length, A.match_length <= A.max_lazy_match && A.lookahead >= 3) {
                A.match_length--;
                do A.strstart++, A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[A.strstart + 3 - 1]), e = A.prev[A.strstart & A.w_mask] = A.head[A.ins_h], A.head[A.ins_h] = A.strstart;
                while (0 != --A.match_length);
                A.strstart++;
            } else A.strstart += A.match_length, A.match_length = 0, A.ins_h = A.window[A.strstart], A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[A.strstart + 1]);
        } else i = $ec3de3bad43fb783$var$j(A, 0, A.window[A.strstart]), A.lookahead--, A.strstart++;
        if (i && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out)) return 1;
    }
    return A.insert = A.strstart < 2 ? A.strstart : 2, t === $ec3de3bad43fb783$var$V ? ($ec3de3bad43fb783$var$QA(A, !0), 0 === A.strm.avail_out ? 3 : 4) : A.sym_next && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out) ? 1 : 2;
}, $ec3de3bad43fb783$var$HA = (A, t)=>{
    let e, i, s;
    for(;;){
        if (A.lookahead < $ec3de3bad43fb783$var$cA) {
            if ($ec3de3bad43fb783$var$pA(A), A.lookahead < $ec3de3bad43fb783$var$cA && t === $ec3de3bad43fb783$var$W) return 1;
            if (0 === A.lookahead) break;
        }
        if (e = 0, A.lookahead >= 3 && (A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[A.strstart + 3 - 1]), e = A.prev[A.strstart & A.w_mask] = A.head[A.ins_h], A.head[A.ins_h] = A.strstart), A.prev_length = A.match_length, A.prev_match = A.match_start, A.match_length = 2, 0 !== e && A.prev_length < A.max_lazy_match && A.strstart - e <= A.w_size - $ec3de3bad43fb783$var$cA && (A.match_length = $ec3de3bad43fb783$var$uA(A, e), A.match_length <= 5 && (A.strategy === $ec3de3bad43fb783$var$nA || 3 === A.match_length && A.strstart - A.match_start > 4096) && (A.match_length = 2)), A.prev_length >= 3 && A.match_length <= A.prev_length) {
            s = A.strstart + A.lookahead - 3, i = $ec3de3bad43fb783$var$j(A, A.strstart - 1 - A.prev_match, A.prev_length - 3), A.lookahead -= A.prev_length - 1, A.prev_length -= 2;
            do ++A.strstart <= s && (A.ins_h = $ec3de3bad43fb783$var$RA(A, A.ins_h, A.window[A.strstart + 3 - 1]), e = A.prev[A.strstart & A.w_mask] = A.head[A.ins_h], A.head[A.ins_h] = A.strstart);
            while (0 != --A.prev_length);
            if (A.match_available = 0, A.match_length = 2, A.strstart++, i && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out)) return 1;
        } else if (A.match_available) {
            if (i = $ec3de3bad43fb783$var$j(A, 0, A.window[A.strstart - 1]), i && $ec3de3bad43fb783$var$QA(A, !1), A.strstart++, A.lookahead--, 0 === A.strm.avail_out) return 1;
        } else A.match_available = 1, A.strstart++, A.lookahead--;
    }
    return A.match_available && (i = $ec3de3bad43fb783$var$j(A, 0, A.window[A.strstart - 1]), A.match_available = 0), A.insert = A.strstart < 2 ? A.strstart : 2, t === $ec3de3bad43fb783$var$V ? ($ec3de3bad43fb783$var$QA(A, !0), 0 === A.strm.avail_out ? 3 : 4) : A.sym_next && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out) ? 1 : 2;
};
function $ec3de3bad43fb783$var$PA(A, t, e, i, s) {
    this.good_length = A, this.max_lazy = t, this.nice_length = e, this.max_chain = i, this.func = s;
}
const $ec3de3bad43fb783$var$OA = [
    new $ec3de3bad43fb783$var$PA(0, 0, 0, 0, $ec3de3bad43fb783$var$yA),
    new $ec3de3bad43fb783$var$PA(4, 4, 8, 4, $ec3de3bad43fb783$var$kA),
    new $ec3de3bad43fb783$var$PA(4, 5, 16, 8, $ec3de3bad43fb783$var$kA),
    new $ec3de3bad43fb783$var$PA(4, 6, 32, 32, $ec3de3bad43fb783$var$kA),
    new $ec3de3bad43fb783$var$PA(4, 4, 16, 16, $ec3de3bad43fb783$var$HA),
    new $ec3de3bad43fb783$var$PA(8, 16, 32, 32, $ec3de3bad43fb783$var$HA),
    new $ec3de3bad43fb783$var$PA(8, 16, 128, 128, $ec3de3bad43fb783$var$HA),
    new $ec3de3bad43fb783$var$PA(8, 32, 128, 256, $ec3de3bad43fb783$var$HA),
    new $ec3de3bad43fb783$var$PA(32, 128, 258, 1024, $ec3de3bad43fb783$var$HA),
    new $ec3de3bad43fb783$var$PA(32, 258, 258, 4096, $ec3de3bad43fb783$var$HA)
];
function $ec3de3bad43fb783$var$UA() {
    this.strm = null, this.status = 0, this.pending_buf = null, this.pending_buf_size = 0, this.pending_out = 0, this.pending = 0, this.wrap = 0, this.gzhead = null, this.gzindex = 0, this.method = $ec3de3bad43fb783$var$BA, this.last_flush = -1, this.w_size = 0, this.w_bits = 0, this.w_mask = 0, this.window = null, this.window_size = 0, this.prev = null, this.head = null, this.ins_h = 0, this.hash_size = 0, this.hash_bits = 0, this.hash_mask = 0, this.hash_shift = 0, this.block_start = 0, this.match_length = 0, this.prev_match = 0, this.match_available = 0, this.strstart = 0, this.match_start = 0, this.lookahead = 0, this.prev_length = 0, this.max_chain_length = 0, this.max_lazy_match = 0, this.level = 0, this.strategy = 0, this.good_match = 0, this.nice_match = 0, this.dyn_ltree = new Uint16Array(1146), this.dyn_dtree = new Uint16Array(122), this.bl_tree = new Uint16Array(78), $ec3de3bad43fb783$var$MA(this.dyn_ltree), $ec3de3bad43fb783$var$MA(this.dyn_dtree), $ec3de3bad43fb783$var$MA(this.bl_tree), this.l_desc = null, this.d_desc = null, this.bl_desc = null, this.bl_count = new Uint16Array(16), this.heap = new Uint16Array(573), $ec3de3bad43fb783$var$MA(this.heap), this.heap_len = 0, this.heap_max = 0, this.depth = new Uint16Array(573), $ec3de3bad43fb783$var$MA(this.depth), this.sym_buf = 0, this.lit_bufsize = 0, this.sym_next = 0, this.sym_end = 0, this.opt_len = 0, this.static_len = 0, this.matches = 0, this.insert = 0, this.bi_buf = 0, this.bi_valid = 0;
}
const $ec3de3bad43fb783$var$GA = (A)=>{
    if (!A) return 1;
    const t = A.state;
    return !t || t.strm !== A || t.status !== $ec3de3bad43fb783$var$CA && 57 !== t.status && 69 !== t.status && 73 !== t.status && 91 !== t.status && 103 !== t.status && t.status !== $ec3de3bad43fb783$var$IA && t.status !== $ec3de3bad43fb783$var$_A ? 1 : 0;
}, $ec3de3bad43fb783$var$mA = (A)=>{
    if ($ec3de3bad43fb783$var$GA(A)) return $ec3de3bad43fb783$var$lA(A, $ec3de3bad43fb783$var$eA);
    A.total_in = A.total_out = 0, A.data_type = $ec3de3bad43fb783$var$oA;
    const t = A.state;
    return t.pending = 0, t.pending_out = 0, t.wrap < 0 && (t.wrap = -t.wrap), t.status = 2 === t.wrap ? 57 : t.wrap ? $ec3de3bad43fb783$var$CA : $ec3de3bad43fb783$var$IA, A.adler = 2 === t.wrap ? 0 : 1, t.last_flush = -2, $ec3de3bad43fb783$var$z(t), $ec3de3bad43fb783$var$AA;
}, $ec3de3bad43fb783$var$YA = (A)=>{
    const t = $ec3de3bad43fb783$var$mA(A);
    var e;
    return t === $ec3de3bad43fb783$var$AA && ((e = A.state).window_size = 2 * e.w_size, $ec3de3bad43fb783$var$MA(e.head), e.max_lazy_match = $ec3de3bad43fb783$var$OA[e.level].max_lazy, e.good_match = $ec3de3bad43fb783$var$OA[e.level].good_length, e.nice_match = $ec3de3bad43fb783$var$OA[e.level].nice_length, e.max_chain_length = $ec3de3bad43fb783$var$OA[e.level].max_chain, e.strstart = 0, e.block_start = 0, e.lookahead = 0, e.insert = 0, e.match_length = e.prev_length = 2, e.match_available = 0, e.ins_h = 0), t;
}, $ec3de3bad43fb783$var$bA = (A, t, e, i, s, a)=>{
    if (!A) return $ec3de3bad43fb783$var$eA;
    let n = 1;
    if (t === $ec3de3bad43fb783$var$aA && (t = 6), i < 0 ? (n = 0, i = -i) : i > 15 && (n = 2, i -= 16), s < 1 || s > 9 || e !== $ec3de3bad43fb783$var$BA || i < 8 || i > 15 || t < 0 || t > 9 || a < 0 || a > $ec3de3bad43fb783$var$rA || 8 === i && 1 !== n) return $ec3de3bad43fb783$var$lA(A, $ec3de3bad43fb783$var$eA);
    8 === i && (i = 9);
    const E = new $ec3de3bad43fb783$var$UA;
    return A.state = E, E.strm = A, E.status = $ec3de3bad43fb783$var$CA, E.wrap = n, E.gzhead = null, E.w_bits = i, E.w_size = 1 << E.w_bits, E.w_mask = E.w_size - 1, E.hash_bits = s + 7, E.hash_size = 1 << E.hash_bits, E.hash_mask = E.hash_size - 1, E.hash_shift = ~~((E.hash_bits + 3 - 1) / 3), E.window = new Uint8Array(2 * E.w_size), E.head = new Uint16Array(E.hash_size), E.prev = new Uint16Array(E.w_size), E.lit_bufsize = 1 << s + 6, E.pending_buf_size = 4 * E.lit_bufsize, E.pending_buf = new Uint8Array(E.pending_buf_size), E.sym_buf = E.lit_bufsize, E.sym_end = 3 * (E.lit_bufsize - 1), E.level = t, E.strategy = a, E.method = e, $ec3de3bad43fb783$var$YA(A);
};
var $ec3de3bad43fb783$var$KA = {
    deflateInit: (A, t)=>$ec3de3bad43fb783$var$bA(A, t, $ec3de3bad43fb783$var$BA, 15, 8, $ec3de3bad43fb783$var$gA),
    deflateInit2: $ec3de3bad43fb783$var$bA,
    deflateReset: $ec3de3bad43fb783$var$YA,
    deflateResetKeep: $ec3de3bad43fb783$var$mA,
    deflateSetHeader: (A, t)=>$ec3de3bad43fb783$var$GA(A) || 2 !== A.state.wrap ? $ec3de3bad43fb783$var$eA : (A.state.gzhead = t, $ec3de3bad43fb783$var$AA),
    deflate: (A, t)=>{
        if ($ec3de3bad43fb783$var$GA(A) || t > $ec3de3bad43fb783$var$$ || t < 0) return A ? $ec3de3bad43fb783$var$lA(A, $ec3de3bad43fb783$var$eA) : $ec3de3bad43fb783$var$eA;
        const e = A.state;
        if (!A.output || 0 !== A.avail_in && !A.input || e.status === $ec3de3bad43fb783$var$_A && t !== $ec3de3bad43fb783$var$V) return $ec3de3bad43fb783$var$lA(A, 0 === A.avail_out ? $ec3de3bad43fb783$var$sA : $ec3de3bad43fb783$var$eA);
        const i = e.last_flush;
        if (e.last_flush = t, 0 !== e.pending) {
            if ($ec3de3bad43fb783$var$SA(A), 0 === A.avail_out) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
        } else if (0 === A.avail_in && $ec3de3bad43fb783$var$dA(t) <= $ec3de3bad43fb783$var$dA(i) && t !== $ec3de3bad43fb783$var$V) return $ec3de3bad43fb783$var$lA(A, $ec3de3bad43fb783$var$sA);
        if (e.status === $ec3de3bad43fb783$var$_A && 0 !== A.avail_in) return $ec3de3bad43fb783$var$lA(A, $ec3de3bad43fb783$var$sA);
        if (e.status === $ec3de3bad43fb783$var$CA && 0 === e.wrap && (e.status = $ec3de3bad43fb783$var$IA), e.status === $ec3de3bad43fb783$var$CA) {
            let t = $ec3de3bad43fb783$var$BA + (e.w_bits - 8 << 4) << 8, i = -1;
            if (i = e.strategy >= $ec3de3bad43fb783$var$EA || e.level < 2 ? 0 : e.level < 6 ? 1 : 6 === e.level ? 2 : 3, t |= i << 6, 0 !== e.strstart && (t |= 32), t += 31 - t % 31, $ec3de3bad43fb783$var$FA(e, t), 0 !== e.strstart && ($ec3de3bad43fb783$var$FA(e, A.adler >>> 16), $ec3de3bad43fb783$var$FA(e, 65535 & A.adler)), A.adler = 1, e.status = $ec3de3bad43fb783$var$IA, $ec3de3bad43fb783$var$SA(A), 0 !== e.pending) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
        }
        if (57 === e.status) {
            if (A.adler = 0, $ec3de3bad43fb783$var$fA(e, 31), $ec3de3bad43fb783$var$fA(e, 139), $ec3de3bad43fb783$var$fA(e, 8), e.gzhead) $ec3de3bad43fb783$var$fA(e, (e.gzhead.text ? 1 : 0) + (e.gzhead.hcrc ? 2 : 0) + (e.gzhead.extra ? 4 : 0) + (e.gzhead.name ? 8 : 0) + (e.gzhead.comment ? 16 : 0)), $ec3de3bad43fb783$var$fA(e, 255 & e.gzhead.time), $ec3de3bad43fb783$var$fA(e, e.gzhead.time >> 8 & 255), $ec3de3bad43fb783$var$fA(e, e.gzhead.time >> 16 & 255), $ec3de3bad43fb783$var$fA(e, e.gzhead.time >> 24 & 255), $ec3de3bad43fb783$var$fA(e, 9 === e.level ? 2 : e.strategy >= $ec3de3bad43fb783$var$EA || e.level < 2 ? 4 : 0), $ec3de3bad43fb783$var$fA(e, 255 & e.gzhead.os), e.gzhead.extra && e.gzhead.extra.length && ($ec3de3bad43fb783$var$fA(e, 255 & e.gzhead.extra.length), $ec3de3bad43fb783$var$fA(e, e.gzhead.extra.length >> 8 & 255)), e.gzhead.hcrc && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending, 0)), e.gzindex = 0, e.status = 69;
            else if ($ec3de3bad43fb783$var$fA(e, 0), $ec3de3bad43fb783$var$fA(e, 0), $ec3de3bad43fb783$var$fA(e, 0), $ec3de3bad43fb783$var$fA(e, 0), $ec3de3bad43fb783$var$fA(e, 0), $ec3de3bad43fb783$var$fA(e, 9 === e.level ? 2 : e.strategy >= $ec3de3bad43fb783$var$EA || e.level < 2 ? 4 : 0), $ec3de3bad43fb783$var$fA(e, 3), e.status = $ec3de3bad43fb783$var$IA, $ec3de3bad43fb783$var$SA(A), 0 !== e.pending) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
        }
        if (69 === e.status) {
            if (e.gzhead.extra) {
                let t = e.pending, i = (65535 & e.gzhead.extra.length) - e.gzindex;
                for(; e.pending + i > e.pending_buf_size;){
                    let s = e.pending_buf_size - e.pending;
                    if (e.pending_buf.set(e.gzhead.extra.subarray(e.gzindex, e.gzindex + s), e.pending), e.pending = e.pending_buf_size, e.gzhead.hcrc && e.pending > t && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending - t, t)), e.gzindex += s, $ec3de3bad43fb783$var$SA(A), 0 !== e.pending) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
                    t = 0, i -= s;
                }
                let s = new Uint8Array(e.gzhead.extra);
                e.pending_buf.set(s.subarray(e.gzindex, e.gzindex + i), e.pending), e.pending += i, e.gzhead.hcrc && e.pending > t && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending - t, t)), e.gzindex = 0;
            }
            e.status = 73;
        }
        if (73 === e.status) {
            if (e.gzhead.name) {
                let t, i = e.pending;
                do {
                    if (e.pending === e.pending_buf_size) {
                        if (e.gzhead.hcrc && e.pending > i && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending - i, i)), $ec3de3bad43fb783$var$SA(A), 0 !== e.pending) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
                        i = 0;
                    }
                    t = e.gzindex < e.gzhead.name.length ? 255 & e.gzhead.name.charCodeAt(e.gzindex++) : 0, $ec3de3bad43fb783$var$fA(e, t);
                }while (0 !== t);
                e.gzhead.hcrc && e.pending > i && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending - i, i)), e.gzindex = 0;
            }
            e.status = 91;
        }
        if (91 === e.status) {
            if (e.gzhead.comment) {
                let t, i = e.pending;
                do {
                    if (e.pending === e.pending_buf_size) {
                        if (e.gzhead.hcrc && e.pending > i && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending - i, i)), $ec3de3bad43fb783$var$SA(A), 0 !== e.pending) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
                        i = 0;
                    }
                    t = e.gzindex < e.gzhead.comment.length ? 255 & e.gzhead.comment.charCodeAt(e.gzindex++) : 0, $ec3de3bad43fb783$var$fA(e, t);
                }while (0 !== t);
                e.gzhead.hcrc && e.pending > i && (A.adler = $ec3de3bad43fb783$var$x(A.adler, e.pending_buf, e.pending - i, i));
            }
            e.status = 103;
        }
        if (103 === e.status) {
            if (e.gzhead.hcrc) {
                if (e.pending + 2 > e.pending_buf_size && ($ec3de3bad43fb783$var$SA(A), 0 !== e.pending)) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
                $ec3de3bad43fb783$var$fA(e, 255 & A.adler), $ec3de3bad43fb783$var$fA(e, A.adler >> 8 & 255), A.adler = 0;
            }
            if (e.status = $ec3de3bad43fb783$var$IA, $ec3de3bad43fb783$var$SA(A), 0 !== e.pending) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
        }
        if (0 !== A.avail_in || 0 !== e.lookahead || t !== $ec3de3bad43fb783$var$W && e.status !== $ec3de3bad43fb783$var$_A) {
            let i = 0 === e.level ? $ec3de3bad43fb783$var$yA(e, t) : e.strategy === $ec3de3bad43fb783$var$EA ? ((A, t)=>{
                let e;
                for(;;){
                    if (0 === A.lookahead && ($ec3de3bad43fb783$var$pA(A), 0 === A.lookahead)) {
                        if (t === $ec3de3bad43fb783$var$W) return 1;
                        break;
                    }
                    if (A.match_length = 0, e = $ec3de3bad43fb783$var$j(A, 0, A.window[A.strstart]), A.lookahead--, A.strstart++, e && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out)) return 1;
                }
                return A.insert = 0, t === $ec3de3bad43fb783$var$V ? ($ec3de3bad43fb783$var$QA(A, !0), 0 === A.strm.avail_out ? 3 : 4) : A.sym_next && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out) ? 1 : 2;
            })(e, t) : e.strategy === $ec3de3bad43fb783$var$hA ? ((A, t)=>{
                let e, i, s, a;
                const n = A.window;
                for(;;){
                    if (A.lookahead <= $ec3de3bad43fb783$var$wA) {
                        if ($ec3de3bad43fb783$var$pA(A), A.lookahead <= $ec3de3bad43fb783$var$wA && t === $ec3de3bad43fb783$var$W) return 1;
                        if (0 === A.lookahead) break;
                    }
                    if (A.match_length = 0, A.lookahead >= 3 && A.strstart > 0 && (s = A.strstart - 1, i = n[s], i === n[++s] && i === n[++s] && i === n[++s])) {
                        a = A.strstart + $ec3de3bad43fb783$var$wA;
                        do ;
                        while (i === n[++s] && i === n[++s] && i === n[++s] && i === n[++s] && i === n[++s] && i === n[++s] && i === n[++s] && i === n[++s] && s < a);
                        A.match_length = $ec3de3bad43fb783$var$wA - (a - s), A.match_length > A.lookahead && (A.match_length = A.lookahead);
                    }
                    if (A.match_length >= 3 ? (e = $ec3de3bad43fb783$var$j(A, 1, A.match_length - 3), A.lookahead -= A.match_length, A.strstart += A.match_length, A.match_length = 0) : (e = $ec3de3bad43fb783$var$j(A, 0, A.window[A.strstart]), A.lookahead--, A.strstart++), e && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out)) return 1;
                }
                return A.insert = 0, t === $ec3de3bad43fb783$var$V ? ($ec3de3bad43fb783$var$QA(A, !0), 0 === A.strm.avail_out ? 3 : 4) : A.sym_next && ($ec3de3bad43fb783$var$QA(A, !1), 0 === A.strm.avail_out) ? 1 : 2;
            })(e, t) : $ec3de3bad43fb783$var$OA[e.level].func(e, t);
            if (3 !== i && 4 !== i || (e.status = $ec3de3bad43fb783$var$_A), 1 === i || 3 === i) return 0 === A.avail_out && (e.last_flush = -1), $ec3de3bad43fb783$var$AA;
            if (2 === i && (t === $ec3de3bad43fb783$var$X ? $ec3de3bad43fb783$var$Z(e) : t !== $ec3de3bad43fb783$var$$ && ($ec3de3bad43fb783$var$N(e, 0, 0, !1), t === $ec3de3bad43fb783$var$q && ($ec3de3bad43fb783$var$MA(e.head), 0 === e.lookahead && (e.strstart = 0, e.block_start = 0, e.insert = 0))), $ec3de3bad43fb783$var$SA(A), 0 === A.avail_out)) return e.last_flush = -1, $ec3de3bad43fb783$var$AA;
        }
        return t !== $ec3de3bad43fb783$var$V ? $ec3de3bad43fb783$var$AA : e.wrap <= 0 ? $ec3de3bad43fb783$var$tA : (2 === e.wrap ? ($ec3de3bad43fb783$var$fA(e, 255 & A.adler), $ec3de3bad43fb783$var$fA(e, A.adler >> 8 & 255), $ec3de3bad43fb783$var$fA(e, A.adler >> 16 & 255), $ec3de3bad43fb783$var$fA(e, A.adler >> 24 & 255), $ec3de3bad43fb783$var$fA(e, 255 & A.total_in), $ec3de3bad43fb783$var$fA(e, A.total_in >> 8 & 255), $ec3de3bad43fb783$var$fA(e, A.total_in >> 16 & 255), $ec3de3bad43fb783$var$fA(e, A.total_in >> 24 & 255)) : ($ec3de3bad43fb783$var$FA(e, A.adler >>> 16), $ec3de3bad43fb783$var$FA(e, 65535 & A.adler)), $ec3de3bad43fb783$var$SA(A), e.wrap > 0 && (e.wrap = -e.wrap), 0 !== e.pending ? $ec3de3bad43fb783$var$AA : $ec3de3bad43fb783$var$tA);
    },
    deflateEnd: (A)=>{
        if ($ec3de3bad43fb783$var$GA(A)) return $ec3de3bad43fb783$var$eA;
        const t = A.state.status;
        return A.state = null, t === $ec3de3bad43fb783$var$IA ? $ec3de3bad43fb783$var$lA(A, $ec3de3bad43fb783$var$iA) : $ec3de3bad43fb783$var$AA;
    },
    deflateSetDictionary: (A, t)=>{
        let e = t.length;
        if ($ec3de3bad43fb783$var$GA(A)) return $ec3de3bad43fb783$var$eA;
        const i = A.state, s = i.wrap;
        if (2 === s || 1 === s && i.status !== $ec3de3bad43fb783$var$CA || i.lookahead) return $ec3de3bad43fb783$var$eA;
        if (1 === s && (A.adler = $ec3de3bad43fb783$var$b(A.adler, t, e, 0)), i.wrap = 0, e >= i.w_size) {
            0 === s && ($ec3de3bad43fb783$var$MA(i.head), i.strstart = 0, i.block_start = 0, i.insert = 0);
            let A = new Uint8Array(i.w_size);
            A.set(t.subarray(e - i.w_size, e), 0), t = A, e = i.w_size;
        }
        const a = A.avail_in, n = A.next_in, E = A.input;
        for(A.avail_in = e, A.next_in = 0, A.input = t, $ec3de3bad43fb783$var$pA(i); i.lookahead >= 3;){
            let A = i.strstart, t = i.lookahead - 2;
            do i.ins_h = $ec3de3bad43fb783$var$RA(i, i.ins_h, i.window[A + 3 - 1]), i.prev[A & i.w_mask] = i.head[i.ins_h], i.head[i.ins_h] = A, A++;
            while (--t);
            i.strstart = A, i.lookahead = 2, $ec3de3bad43fb783$var$pA(i);
        }
        return i.strstart += i.lookahead, i.block_start = i.strstart, i.insert = i.lookahead, i.lookahead = 0, i.match_length = i.prev_length = 2, i.match_available = 0, A.next_in = n, A.input = E, A.avail_in = a, i.wrap = s, $ec3de3bad43fb783$var$AA;
    },
    deflateInfo: "pako deflate (from Nodeca project)"
};
const $ec3de3bad43fb783$var$xA = (A, t)=>Object.prototype.hasOwnProperty.call(A, t);
var $ec3de3bad43fb783$var$LA = {
    assign: function(A) {
        const t = Array.prototype.slice.call(arguments, 1);
        for(; t.length;){
            const e = t.shift();
            if (e) {
                if ("object" != typeof e) throw new TypeError(e + "must be non-object");
                for(const t in e)$ec3de3bad43fb783$var$xA(e, t) && (A[t] = e[t]);
            }
        }
        return A;
    },
    flattenChunks: (A)=>{
        let t = 0;
        for(let e = 0, i = A.length; e < i; e++)t += A[e].length;
        const e = new Uint8Array(t);
        for(let t = 0, i = 0, s = A.length; t < s; t++){
            let s = A[t];
            e.set(s, i), i += s.length;
        }
        return e;
    }
};
let $ec3de3bad43fb783$var$JA = !0;
try {
    String.fromCharCode.apply(null, new Uint8Array(1));
} catch (A) {
    $ec3de3bad43fb783$var$JA = !1;
}
const $ec3de3bad43fb783$var$zA = new Uint8Array(256);
for(let A = 0; A < 256; A++)$ec3de3bad43fb783$var$zA[A] = A >= 252 ? 6 : A >= 248 ? 5 : A >= 240 ? 4 : A >= 224 ? 3 : A >= 192 ? 2 : 1;
$ec3de3bad43fb783$var$zA[254] = $ec3de3bad43fb783$var$zA[254] = 1;
var $ec3de3bad43fb783$var$NA = {
    string2buf: (A)=>{
        if ("function" == typeof TextEncoder && TextEncoder.prototype.encode) return (new TextEncoder).encode(A);
        let t, e, i, s, a, n = A.length, E = 0;
        for(s = 0; s < n; s++)e = A.charCodeAt(s), 55296 == (64512 & e) && s + 1 < n && (i = A.charCodeAt(s + 1), 56320 == (64512 & i) && (e = 65536 + (e - 55296 << 10) + (i - 56320), s++)), E += e < 128 ? 1 : e < 2048 ? 2 : e < 65536 ? 3 : 4;
        for(t = new Uint8Array(E), a = 0, s = 0; a < E; s++)e = A.charCodeAt(s), 55296 == (64512 & e) && s + 1 < n && (i = A.charCodeAt(s + 1), 56320 == (64512 & i) && (e = 65536 + (e - 55296 << 10) + (i - 56320), s++)), e < 128 ? t[a++] = e : e < 2048 ? (t[a++] = 192 | e >>> 6, t[a++] = 128 | 63 & e) : e < 65536 ? (t[a++] = 224 | e >>> 12, t[a++] = 128 | e >>> 6 & 63, t[a++] = 128 | 63 & e) : (t[a++] = 240 | e >>> 18, t[a++] = 128 | e >>> 12 & 63, t[a++] = 128 | e >>> 6 & 63, t[a++] = 128 | 63 & e);
        return t;
    },
    buf2string: (A, t)=>{
        const e = t || A.length;
        if ("function" == typeof TextDecoder && TextDecoder.prototype.decode) return (new TextDecoder).decode(A.subarray(0, t));
        let i, s;
        const a = new Array(2 * e);
        for(s = 0, i = 0; i < e;){
            let t = A[i++];
            if (t < 128) {
                a[s++] = t;
                continue;
            }
            let n = $ec3de3bad43fb783$var$zA[t];
            if (n > 4) a[s++] = 65533, i += n - 1;
            else {
                for(t &= 2 === n ? 31 : 3 === n ? 15 : 7; n > 1 && i < e;)t = t << 6 | 63 & A[i++], n--;
                n > 1 ? a[s++] = 65533 : t < 65536 ? a[s++] = t : (t -= 65536, a[s++] = 55296 | t >> 10 & 1023, a[s++] = 56320 | 1023 & t);
            }
        }
        return ((A, t)=>{
            if (t < 65534 && A.subarray && $ec3de3bad43fb783$var$JA) return String.fromCharCode.apply(null, A.length === t ? A : A.subarray(0, t));
            let e = "";
            for(let i = 0; i < t; i++)e += String.fromCharCode(A[i]);
            return e;
        })(a, s);
    },
    utf8border: (A, t)=>{
        (t = t || A.length) > A.length && (t = A.length);
        let e = t - 1;
        for(; e >= 0 && 128 == (192 & A[e]);)e--;
        return e < 0 || 0 === e ? t : e + $ec3de3bad43fb783$var$zA[A[e]] > t ? e : t;
    }
};
var $ec3de3bad43fb783$var$vA = function() {
    this.input = null, this.next_in = 0, this.avail_in = 0, this.total_in = 0, this.output = null, this.next_out = 0, this.avail_out = 0, this.total_out = 0, this.msg = "", this.state = null, this.data_type = 2, this.adler = 0;
};
const $ec3de3bad43fb783$var$jA = Object.prototype.toString, { Z_NO_FLUSH: $ec3de3bad43fb783$var$ZA, Z_SYNC_FLUSH: $ec3de3bad43fb783$var$WA, Z_FULL_FLUSH: $ec3de3bad43fb783$var$XA, Z_FINISH: $ec3de3bad43fb783$var$qA, Z_OK: $ec3de3bad43fb783$var$VA, Z_STREAM_END: $ec3de3bad43fb783$var$$A, Z_DEFAULT_COMPRESSION: $ec3de3bad43fb783$var$At, Z_DEFAULT_STRATEGY: $ec3de3bad43fb783$var$tt, Z_DEFLATED: $ec3de3bad43fb783$var$et } = $ec3de3bad43fb783$var$J;
function $ec3de3bad43fb783$var$it(A) {
    this.options = $ec3de3bad43fb783$var$LA.assign({
        level: $ec3de3bad43fb783$var$At,
        method: $ec3de3bad43fb783$var$et,
        chunkSize: 16384,
        windowBits: 15,
        memLevel: 8,
        strategy: $ec3de3bad43fb783$var$tt
    }, A || {});
    let t = this.options;
    t.raw && t.windowBits > 0 ? t.windowBits = -t.windowBits : t.gzip && t.windowBits > 0 && t.windowBits < 16 && (t.windowBits += 16), this.err = 0, this.msg = "", this.ended = !1, this.chunks = [], this.strm = new $ec3de3bad43fb783$var$vA, this.strm.avail_out = 0;
    let e = $ec3de3bad43fb783$var$KA.deflateInit2(this.strm, t.level, t.method, t.windowBits, t.memLevel, t.strategy);
    if (e !== $ec3de3bad43fb783$var$VA) throw new Error($ec3de3bad43fb783$var$L[e]);
    if (t.header && $ec3de3bad43fb783$var$KA.deflateSetHeader(this.strm, t.header), t.dictionary) {
        let A;
        if (A = "string" == typeof t.dictionary ? $ec3de3bad43fb783$var$NA.string2buf(t.dictionary) : "[object ArrayBuffer]" === $ec3de3bad43fb783$var$jA.call(t.dictionary) ? new Uint8Array(t.dictionary) : t.dictionary, e = $ec3de3bad43fb783$var$KA.deflateSetDictionary(this.strm, A), e !== $ec3de3bad43fb783$var$VA) throw new Error($ec3de3bad43fb783$var$L[e]);
        this._dict_set = !0;
    }
}
function $ec3de3bad43fb783$var$st(A, t) {
    const e = new $ec3de3bad43fb783$var$it(t);
    if (e.push(A, !0), e.err) throw e.msg || $ec3de3bad43fb783$var$L[e.err];
    return e.result;
}
$ec3de3bad43fb783$var$it.prototype.push = function(A, t) {
    const e = this.strm, i = this.options.chunkSize;
    let s, a;
    if (this.ended) return !1;
    for(a = t === ~~t ? t : !0 === t ? $ec3de3bad43fb783$var$qA : $ec3de3bad43fb783$var$ZA, "string" == typeof A ? e.input = $ec3de3bad43fb783$var$NA.string2buf(A) : "[object ArrayBuffer]" === $ec3de3bad43fb783$var$jA.call(A) ? e.input = new Uint8Array(A) : e.input = A, e.next_in = 0, e.avail_in = e.input.length;;)if (0 === e.avail_out && (e.output = new Uint8Array(i), e.next_out = 0, e.avail_out = i), (a === $ec3de3bad43fb783$var$WA || a === $ec3de3bad43fb783$var$XA) && e.avail_out <= 6) this.onData(e.output.subarray(0, e.next_out)), e.avail_out = 0;
    else {
        if (s = $ec3de3bad43fb783$var$KA.deflate(e, a), s === $ec3de3bad43fb783$var$$A) return e.next_out > 0 && this.onData(e.output.subarray(0, e.next_out)), s = $ec3de3bad43fb783$var$KA.deflateEnd(this.strm), this.onEnd(s), this.ended = !0, s === $ec3de3bad43fb783$var$VA;
        if (0 !== e.avail_out) {
            if (a > 0 && e.next_out > 0) this.onData(e.output.subarray(0, e.next_out)), e.avail_out = 0;
            else if (0 === e.avail_in) break;
        } else this.onData(e.output);
    }
    return !0;
}, $ec3de3bad43fb783$var$it.prototype.onData = function(A) {
    this.chunks.push(A);
}, $ec3de3bad43fb783$var$it.prototype.onEnd = function(A) {
    A === $ec3de3bad43fb783$var$VA && (this.result = $ec3de3bad43fb783$var$LA.flattenChunks(this.chunks)), this.chunks = [], this.err = A, this.msg = this.strm.msg;
};
var $ec3de3bad43fb783$var$at = {
    Deflate: $ec3de3bad43fb783$var$it,
    deflate: $ec3de3bad43fb783$var$st,
    deflateRaw: function(A, t) {
        return (t = t || {}).raw = !0, $ec3de3bad43fb783$var$st(A, t);
    },
    gzip: function(A, t) {
        return (t = t || {}).gzip = !0, $ec3de3bad43fb783$var$st(A, t);
    },
    constants: $ec3de3bad43fb783$var$J
};
const $ec3de3bad43fb783$var$nt = 16209;
var $ec3de3bad43fb783$var$Et = function(A, t) {
    let e, i, s, a, n, E, h, r, g, o, B, w, c, C, I, _, l, d, M, D, R, S, Q, f;
    const F = A.state;
    e = A.next_in, Q = A.input, i = e + (A.avail_in - 5), s = A.next_out, f = A.output, a = s - (t - A.avail_out), n = s + (A.avail_out - 257), E = F.dmax, h = F.wsize, r = F.whave, g = F.wnext, o = F.window, B = F.hold, w = F.bits, c = F.lencode, C = F.distcode, I = (1 << F.lenbits) - 1, _ = (1 << F.distbits) - 1;
    A: do {
        w < 15 && (B += Q[e++] << w, w += 8, B += Q[e++] << w, w += 8), l = c[B & I];
        t: for(;;){
            if (d = l >>> 24, B >>>= d, w -= d, d = l >>> 16 & 255, 0 === d) f[s++] = 65535 & l;
            else {
                if (!(16 & d)) {
                    if (0 == (64 & d)) {
                        l = c[(65535 & l) + (B & (1 << d) - 1)];
                        continue t;
                    }
                    if (32 & d) {
                        F.mode = 16191;
                        break A;
                    }
                    A.msg = "invalid literal/length code", F.mode = $ec3de3bad43fb783$var$nt;
                    break A;
                }
                M = 65535 & l, d &= 15, d && (w < d && (B += Q[e++] << w, w += 8), M += B & (1 << d) - 1, B >>>= d, w -= d), w < 15 && (B += Q[e++] << w, w += 8, B += Q[e++] << w, w += 8), l = C[B & _];
                e: for(;;){
                    if (d = l >>> 24, B >>>= d, w -= d, d = l >>> 16 & 255, !(16 & d)) {
                        if (0 == (64 & d)) {
                            l = C[(65535 & l) + (B & (1 << d) - 1)];
                            continue e;
                        }
                        A.msg = "invalid distance code", F.mode = $ec3de3bad43fb783$var$nt;
                        break A;
                    }
                    if (D = 65535 & l, d &= 15, w < d && (B += Q[e++] << w, w += 8, w < d && (B += Q[e++] << w, w += 8)), D += B & (1 << d) - 1, D > E) {
                        A.msg = "invalid distance too far back", F.mode = $ec3de3bad43fb783$var$nt;
                        break A;
                    }
                    if (B >>>= d, w -= d, d = s - a, D > d) {
                        if (d = D - d, d > r && F.sane) {
                            A.msg = "invalid distance too far back", F.mode = $ec3de3bad43fb783$var$nt;
                            break A;
                        }
                        if (R = 0, S = o, 0 === g) {
                            if (R += h - d, d < M) {
                                M -= d;
                                do f[s++] = o[R++];
                                while (--d);
                                R = s - D, S = f;
                            }
                        } else if (g < d) {
                            if (R += h + g - d, d -= g, d < M) {
                                M -= d;
                                do f[s++] = o[R++];
                                while (--d);
                                if (R = 0, g < M) {
                                    d = g, M -= d;
                                    do f[s++] = o[R++];
                                    while (--d);
                                    R = s - D, S = f;
                                }
                            }
                        } else if (R += g - d, d < M) {
                            M -= d;
                            do f[s++] = o[R++];
                            while (--d);
                            R = s - D, S = f;
                        }
                        for(; M > 2;)f[s++] = S[R++], f[s++] = S[R++], f[s++] = S[R++], M -= 3;
                        M && (f[s++] = S[R++], M > 1 && (f[s++] = S[R++]));
                    } else {
                        R = s - D;
                        do f[s++] = f[R++], f[s++] = f[R++], f[s++] = f[R++], M -= 3;
                        while (M > 2);
                        M && (f[s++] = f[R++], M > 1 && (f[s++] = f[R++]));
                    }
                    break;
                }
            }
            break;
        }
    }while (e < i && s < n);
    M = w >> 3, e -= M, w -= M << 3, B &= (1 << w) - 1, A.next_in = e, A.next_out = s, A.avail_in = e < i ? i - e + 5 : 5 - (e - i), A.avail_out = s < n ? n - s + 257 : 257 - (s - n), F.hold = B, F.bits = w;
};
const $ec3de3bad43fb783$var$ht = 15, $ec3de3bad43fb783$var$rt = new Uint16Array([
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    13,
    15,
    17,
    19,
    23,
    27,
    31,
    35,
    43,
    51,
    59,
    67,
    83,
    99,
    115,
    131,
    163,
    195,
    227,
    258,
    0,
    0
]), $ec3de3bad43fb783$var$gt = new Uint8Array([
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    17,
    17,
    17,
    17,
    18,
    18,
    18,
    18,
    19,
    19,
    19,
    19,
    20,
    20,
    20,
    20,
    21,
    21,
    21,
    21,
    16,
    72,
    78
]), $ec3de3bad43fb783$var$ot = new Uint16Array([
    1,
    2,
    3,
    4,
    5,
    7,
    9,
    13,
    17,
    25,
    33,
    49,
    65,
    97,
    129,
    193,
    257,
    385,
    513,
    769,
    1025,
    1537,
    2049,
    3073,
    4097,
    6145,
    8193,
    12289,
    16385,
    24577,
    0,
    0
]), $ec3de3bad43fb783$var$Bt = new Uint8Array([
    16,
    16,
    16,
    16,
    17,
    17,
    18,
    18,
    19,
    19,
    20,
    20,
    21,
    21,
    22,
    22,
    23,
    23,
    24,
    24,
    25,
    25,
    26,
    26,
    27,
    27,
    28,
    28,
    29,
    29,
    64,
    64
]);
var $ec3de3bad43fb783$var$wt = (A, t, e, i, s, a, n, E)=>{
    const h = E.bits;
    let r, g, o, B, w, c, C = 0, I = 0, _ = 0, l = 0, d = 0, M = 0, D = 0, R = 0, S = 0, Q = 0, f = null;
    const F = new Uint16Array(16), T = new Uint16Array(16);
    let u, p, y, k = null;
    for(C = 0; C <= $ec3de3bad43fb783$var$ht; C++)F[C] = 0;
    for(I = 0; I < i; I++)F[t[e + I]]++;
    for(d = h, l = $ec3de3bad43fb783$var$ht; l >= 1 && 0 === F[l]; l--);
    if (d > l && (d = l), 0 === l) return s[a++] = 20971520, s[a++] = 20971520, E.bits = 1, 0;
    for(_ = 1; _ < l && 0 === F[_]; _++);
    for(d < _ && (d = _), R = 1, C = 1; C <= $ec3de3bad43fb783$var$ht; C++)if (R <<= 1, R -= F[C], R < 0) return -1;
    if (R > 0 && (0 === A || 1 !== l)) return -1;
    for(T[1] = 0, C = 1; C < $ec3de3bad43fb783$var$ht; C++)T[C + 1] = T[C] + F[C];
    for(I = 0; I < i; I++)0 !== t[e + I] && (n[T[t[e + I]]++] = I);
    if (0 === A ? (f = k = n, c = 20) : 1 === A ? (f = $ec3de3bad43fb783$var$rt, k = $ec3de3bad43fb783$var$gt, c = 257) : (f = $ec3de3bad43fb783$var$ot, k = $ec3de3bad43fb783$var$Bt, c = 0), Q = 0, I = 0, C = _, w = a, M = d, D = 0, o = -1, S = 1 << d, B = S - 1, 1 === A && S > 852 || 2 === A && S > 592) return 1;
    for(;;){
        u = C - D, n[I] + 1 < c ? (p = 0, y = n[I]) : n[I] >= c ? (p = k[n[I] - c], y = f[n[I] - c]) : (p = 96, y = 0), r = 1 << C - D, g = 1 << M, _ = g;
        do g -= r, s[w + (Q >> D) + g] = u << 24 | p << 16 | y | 0;
        while (0 !== g);
        for(r = 1 << C - 1; Q & r;)r >>= 1;
        if (0 !== r ? (Q &= r - 1, Q += r) : Q = 0, I++, 0 == --F[C]) {
            if (C === l) break;
            C = t[e + n[I]];
        }
        if (C > d && (Q & B) !== o) {
            for(0 === D && (D = d), w += _, M = C - D, R = 1 << M; M + D < l && (R -= F[M + D], !(R <= 0));)M++, R <<= 1;
            if (S += 1 << M, 1 === A && S > 852 || 2 === A && S > 592) return 1;
            o = Q & B, s[o] = d << 24 | M << 16 | w - a | 0;
        }
    }
    return 0 !== Q && (s[w + Q] = C - D << 24 | 4194304), E.bits = d, 0;
};
const { Z_FINISH: $ec3de3bad43fb783$var$ct, Z_BLOCK: $ec3de3bad43fb783$var$Ct, Z_TREES: $ec3de3bad43fb783$var$It, Z_OK: $ec3de3bad43fb783$var$_t, Z_STREAM_END: $ec3de3bad43fb783$var$lt, Z_NEED_DICT: $ec3de3bad43fb783$var$dt, Z_STREAM_ERROR: $ec3de3bad43fb783$var$Mt, Z_DATA_ERROR: $ec3de3bad43fb783$var$Dt, Z_MEM_ERROR: $ec3de3bad43fb783$var$Rt, Z_BUF_ERROR: $ec3de3bad43fb783$var$St, Z_DEFLATED: $ec3de3bad43fb783$var$Qt } = $ec3de3bad43fb783$var$J, $ec3de3bad43fb783$var$ft = 16180, $ec3de3bad43fb783$var$Ft = 16190, $ec3de3bad43fb783$var$Tt = 16191, $ec3de3bad43fb783$var$ut = 16192, $ec3de3bad43fb783$var$pt = 16194, $ec3de3bad43fb783$var$yt = 16199, $ec3de3bad43fb783$var$kt = 16200, $ec3de3bad43fb783$var$Ht = 16206, $ec3de3bad43fb783$var$Pt = 16209, $ec3de3bad43fb783$var$Ot = (A)=>(A >>> 24 & 255) + (A >>> 8 & 65280) + ((65280 & A) << 8) + ((255 & A) << 24);
function $ec3de3bad43fb783$var$Ut() {
    this.strm = null, this.mode = 0, this.last = !1, this.wrap = 0, this.havedict = !1, this.flags = 0, this.dmax = 0, this.check = 0, this.total = 0, this.head = null, this.wbits = 0, this.wsize = 0, this.whave = 0, this.wnext = 0, this.window = null, this.hold = 0, this.bits = 0, this.length = 0, this.offset = 0, this.extra = 0, this.lencode = null, this.distcode = null, this.lenbits = 0, this.distbits = 0, this.ncode = 0, this.nlen = 0, this.ndist = 0, this.have = 0, this.next = null, this.lens = new Uint16Array(320), this.work = new Uint16Array(288), this.lendyn = null, this.distdyn = null, this.sane = 0, this.back = 0, this.was = 0;
}
const $ec3de3bad43fb783$var$Gt = (A)=>{
    if (!A) return 1;
    const t = A.state;
    return !t || t.strm !== A || t.mode < $ec3de3bad43fb783$var$ft || t.mode > 16211 ? 1 : 0;
}, $ec3de3bad43fb783$var$mt = (A)=>{
    if ($ec3de3bad43fb783$var$Gt(A)) return $ec3de3bad43fb783$var$Mt;
    const t = A.state;
    return A.total_in = A.total_out = t.total = 0, A.msg = "", t.wrap && (A.adler = 1 & t.wrap), t.mode = $ec3de3bad43fb783$var$ft, t.last = 0, t.havedict = 0, t.flags = -1, t.dmax = 32768, t.head = null, t.hold = 0, t.bits = 0, t.lencode = t.lendyn = new Int32Array(852), t.distcode = t.distdyn = new Int32Array(592), t.sane = 1, t.back = -1, $ec3de3bad43fb783$var$_t;
}, $ec3de3bad43fb783$var$Yt = (A)=>{
    if ($ec3de3bad43fb783$var$Gt(A)) return $ec3de3bad43fb783$var$Mt;
    const t = A.state;
    return t.wsize = 0, t.whave = 0, t.wnext = 0, $ec3de3bad43fb783$var$mt(A);
}, $ec3de3bad43fb783$var$bt = (A, t)=>{
    let e;
    if ($ec3de3bad43fb783$var$Gt(A)) return $ec3de3bad43fb783$var$Mt;
    const i = A.state;
    return t < 0 ? (e = 0, t = -t) : (e = 5 + (t >> 4), t < 48 && (t &= 15)), t && (t < 8 || t > 15) ? $ec3de3bad43fb783$var$Mt : (null !== i.window && i.wbits !== t && (i.window = null), i.wrap = e, i.wbits = t, $ec3de3bad43fb783$var$Yt(A));
}, $ec3de3bad43fb783$var$Kt = (A, t)=>{
    if (!A) return $ec3de3bad43fb783$var$Mt;
    const e = new $ec3de3bad43fb783$var$Ut;
    A.state = e, e.strm = A, e.window = null, e.mode = $ec3de3bad43fb783$var$ft;
    const i = $ec3de3bad43fb783$var$bt(A, t);
    return i !== $ec3de3bad43fb783$var$_t && (A.state = null), i;
};
let $ec3de3bad43fb783$var$xt, $ec3de3bad43fb783$var$Lt, $ec3de3bad43fb783$var$Jt = !0;
const $ec3de3bad43fb783$var$zt = (A)=>{
    if ($ec3de3bad43fb783$var$Jt) {
        $ec3de3bad43fb783$var$xt = new Int32Array(512), $ec3de3bad43fb783$var$Lt = new Int32Array(32);
        let t = 0;
        for(; t < 144;)A.lens[t++] = 8;
        for(; t < 256;)A.lens[t++] = 9;
        for(; t < 280;)A.lens[t++] = 7;
        for(; t < 288;)A.lens[t++] = 8;
        for($ec3de3bad43fb783$var$wt(1, A.lens, 0, 288, $ec3de3bad43fb783$var$xt, 0, A.work, {
            bits: 9
        }), t = 0; t < 32;)A.lens[t++] = 5;
        $ec3de3bad43fb783$var$wt(2, A.lens, 0, 32, $ec3de3bad43fb783$var$Lt, 0, A.work, {
            bits: 5
        }), $ec3de3bad43fb783$var$Jt = !1;
    }
    A.lencode = $ec3de3bad43fb783$var$xt, A.lenbits = 9, A.distcode = $ec3de3bad43fb783$var$Lt, A.distbits = 5;
}, $ec3de3bad43fb783$var$Nt = (A, t, e, i)=>{
    let s;
    const a = A.state;
    return null === a.window && (a.wsize = 1 << a.wbits, a.wnext = 0, a.whave = 0, a.window = new Uint8Array(a.wsize)), i >= a.wsize ? (a.window.set(t.subarray(e - a.wsize, e), 0), a.wnext = 0, a.whave = a.wsize) : (s = a.wsize - a.wnext, s > i && (s = i), a.window.set(t.subarray(e - i, e - i + s), a.wnext), (i -= s) ? (a.window.set(t.subarray(e - i, e), 0), a.wnext = i, a.whave = a.wsize) : (a.wnext += s, a.wnext === a.wsize && (a.wnext = 0), a.whave < a.wsize && (a.whave += s))), 0;
};
var $ec3de3bad43fb783$var$vt = {
    inflateReset: $ec3de3bad43fb783$var$Yt,
    inflateReset2: $ec3de3bad43fb783$var$bt,
    inflateResetKeep: $ec3de3bad43fb783$var$mt,
    inflateInit: (A)=>$ec3de3bad43fb783$var$Kt(A, 15),
    inflateInit2: $ec3de3bad43fb783$var$Kt,
    inflate: (A, t)=>{
        let e, i, s, a, n, E, h, r, g, o, B, w, c, C, I, _, l, d, M, D, R, S, Q = 0;
        const f = new Uint8Array(4);
        let F, T;
        const u = new Uint8Array([
            16,
            17,
            18,
            0,
            8,
            7,
            9,
            6,
            10,
            5,
            11,
            4,
            12,
            3,
            13,
            2,
            14,
            1,
            15
        ]);
        if ($ec3de3bad43fb783$var$Gt(A) || !A.output || !A.input && 0 !== A.avail_in) return $ec3de3bad43fb783$var$Mt;
        e = A.state, e.mode === $ec3de3bad43fb783$var$Tt && (e.mode = $ec3de3bad43fb783$var$ut), n = A.next_out, s = A.output, h = A.avail_out, a = A.next_in, i = A.input, E = A.avail_in, r = e.hold, g = e.bits, o = E, B = h, S = $ec3de3bad43fb783$var$_t;
        A: for(;;)switch(e.mode){
            case $ec3de3bad43fb783$var$ft:
                if (0 === e.wrap) {
                    e.mode = $ec3de3bad43fb783$var$ut;
                    break;
                }
                for(; g < 16;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                if (2 & e.wrap && 35615 === r) {
                    0 === e.wbits && (e.wbits = 15), e.check = 0, f[0] = 255 & r, f[1] = r >>> 8 & 255, e.check = $ec3de3bad43fb783$var$x(e.check, f, 2, 0), r = 0, g = 0, e.mode = 16181;
                    break;
                }
                if (e.head && (e.head.done = !1), !(1 & e.wrap) || (((255 & r) << 8) + (r >> 8)) % 31) {
                    A.msg = "incorrect header check", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if ((15 & r) !== $ec3de3bad43fb783$var$Qt) {
                    A.msg = "unknown compression method", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if (r >>>= 4, g -= 4, R = 8 + (15 & r), 0 === e.wbits && (e.wbits = R), R > 15 || R > e.wbits) {
                    A.msg = "invalid window size", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.dmax = 1 << e.wbits, e.flags = 0, A.adler = e.check = 1, e.mode = 512 & r ? 16189 : $ec3de3bad43fb783$var$Tt, r = 0, g = 0;
                break;
            case 16181:
                for(; g < 16;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                if (e.flags = r, (255 & e.flags) !== $ec3de3bad43fb783$var$Qt) {
                    A.msg = "unknown compression method", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if (57344 & e.flags) {
                    A.msg = "unknown header flags set", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.head && (e.head.text = r >> 8 & 1), 512 & e.flags && 4 & e.wrap && (f[0] = 255 & r, f[1] = r >>> 8 & 255, e.check = $ec3de3bad43fb783$var$x(e.check, f, 2, 0)), r = 0, g = 0, e.mode = 16182;
            case 16182:
                for(; g < 32;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                e.head && (e.head.time = r), 512 & e.flags && 4 & e.wrap && (f[0] = 255 & r, f[1] = r >>> 8 & 255, f[2] = r >>> 16 & 255, f[3] = r >>> 24 & 255, e.check = $ec3de3bad43fb783$var$x(e.check, f, 4, 0)), r = 0, g = 0, e.mode = 16183;
            case 16183:
                for(; g < 16;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                e.head && (e.head.xflags = 255 & r, e.head.os = r >> 8), 512 & e.flags && 4 & e.wrap && (f[0] = 255 & r, f[1] = r >>> 8 & 255, e.check = $ec3de3bad43fb783$var$x(e.check, f, 2, 0)), r = 0, g = 0, e.mode = 16184;
            case 16184:
                if (1024 & e.flags) {
                    for(; g < 16;){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    e.length = r, e.head && (e.head.extra_len = r), 512 & e.flags && 4 & e.wrap && (f[0] = 255 & r, f[1] = r >>> 8 & 255, e.check = $ec3de3bad43fb783$var$x(e.check, f, 2, 0)), r = 0, g = 0;
                } else e.head && (e.head.extra = null);
                e.mode = 16185;
            case 16185:
                if (1024 & e.flags && (w = e.length, w > E && (w = E), w && (e.head && (R = e.head.extra_len - e.length, e.head.extra || (e.head.extra = new Uint8Array(e.head.extra_len)), e.head.extra.set(i.subarray(a, a + w), R)), 512 & e.flags && 4 & e.wrap && (e.check = $ec3de3bad43fb783$var$x(e.check, i, w, a)), E -= w, a += w, e.length -= w), e.length)) break A;
                e.length = 0, e.mode = 16186;
            case 16186:
                if (2048 & e.flags) {
                    if (0 === E) break A;
                    w = 0;
                    do R = i[a + w++], e.head && R && e.length < 65536 && (e.head.name += String.fromCharCode(R));
                    while (R && w < E);
                    if (512 & e.flags && 4 & e.wrap && (e.check = $ec3de3bad43fb783$var$x(e.check, i, w, a)), E -= w, a += w, R) break A;
                } else e.head && (e.head.name = null);
                e.length = 0, e.mode = 16187;
            case 16187:
                if (4096 & e.flags) {
                    if (0 === E) break A;
                    w = 0;
                    do R = i[a + w++], e.head && R && e.length < 65536 && (e.head.comment += String.fromCharCode(R));
                    while (R && w < E);
                    if (512 & e.flags && 4 & e.wrap && (e.check = $ec3de3bad43fb783$var$x(e.check, i, w, a)), E -= w, a += w, R) break A;
                } else e.head && (e.head.comment = null);
                e.mode = 16188;
            case 16188:
                if (512 & e.flags) {
                    for(; g < 16;){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    if (4 & e.wrap && r !== (65535 & e.check)) {
                        A.msg = "header crc mismatch", e.mode = $ec3de3bad43fb783$var$Pt;
                        break;
                    }
                    r = 0, g = 0;
                }
                e.head && (e.head.hcrc = e.flags >> 9 & 1, e.head.done = !0), A.adler = e.check = 0, e.mode = $ec3de3bad43fb783$var$Tt;
                break;
            case 16189:
                for(; g < 32;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                A.adler = e.check = $ec3de3bad43fb783$var$Ot(r), r = 0, g = 0, e.mode = $ec3de3bad43fb783$var$Ft;
            case $ec3de3bad43fb783$var$Ft:
                if (0 === e.havedict) return A.next_out = n, A.avail_out = h, A.next_in = a, A.avail_in = E, e.hold = r, e.bits = g, $ec3de3bad43fb783$var$dt;
                A.adler = e.check = 1, e.mode = $ec3de3bad43fb783$var$Tt;
            case $ec3de3bad43fb783$var$Tt:
                if (t === $ec3de3bad43fb783$var$Ct || t === $ec3de3bad43fb783$var$It) break A;
            case $ec3de3bad43fb783$var$ut:
                if (e.last) {
                    r >>>= 7 & g, g -= 7 & g, e.mode = $ec3de3bad43fb783$var$Ht;
                    break;
                }
                for(; g < 3;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                switch(e.last = 1 & r, r >>>= 1, g -= 1, 3 & r){
                    case 0:
                        e.mode = 16193;
                        break;
                    case 1:
                        if ($ec3de3bad43fb783$var$zt(e), e.mode = $ec3de3bad43fb783$var$yt, t === $ec3de3bad43fb783$var$It) {
                            r >>>= 2, g -= 2;
                            break A;
                        }
                        break;
                    case 2:
                        e.mode = 16196;
                        break;
                    case 3:
                        A.msg = "invalid block type", e.mode = $ec3de3bad43fb783$var$Pt;
                }
                r >>>= 2, g -= 2;
                break;
            case 16193:
                for(r >>>= 7 & g, g -= 7 & g; g < 32;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                if ((65535 & r) != (r >>> 16 ^ 65535)) {
                    A.msg = "invalid stored block lengths", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if (e.length = 65535 & r, r = 0, g = 0, e.mode = $ec3de3bad43fb783$var$pt, t === $ec3de3bad43fb783$var$It) break A;
            case $ec3de3bad43fb783$var$pt:
                e.mode = 16195;
            case 16195:
                if (w = e.length, w) {
                    if (w > E && (w = E), w > h && (w = h), 0 === w) break A;
                    s.set(i.subarray(a, a + w), n), E -= w, a += w, h -= w, n += w, e.length -= w;
                    break;
                }
                e.mode = $ec3de3bad43fb783$var$Tt;
                break;
            case 16196:
                for(; g < 14;){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                if (e.nlen = 257 + (31 & r), r >>>= 5, g -= 5, e.ndist = 1 + (31 & r), r >>>= 5, g -= 5, e.ncode = 4 + (15 & r), r >>>= 4, g -= 4, e.nlen > 286 || e.ndist > 30) {
                    A.msg = "too many length or distance symbols", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.have = 0, e.mode = 16197;
            case 16197:
                for(; e.have < e.ncode;){
                    for(; g < 3;){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    e.lens[u[e.have++]] = 7 & r, r >>>= 3, g -= 3;
                }
                for(; e.have < 19;)e.lens[u[e.have++]] = 0;
                if (e.lencode = e.lendyn, e.lenbits = 7, F = {
                    bits: e.lenbits
                }, S = $ec3de3bad43fb783$var$wt(0, e.lens, 0, 19, e.lencode, 0, e.work, F), e.lenbits = F.bits, S) {
                    A.msg = "invalid code lengths set", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.have = 0, e.mode = 16198;
            case 16198:
                for(; e.have < e.nlen + e.ndist;){
                    for(; Q = e.lencode[r & (1 << e.lenbits) - 1], I = Q >>> 24, _ = Q >>> 16 & 255, l = 65535 & Q, !(I <= g);){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    if (l < 16) r >>>= I, g -= I, e.lens[e.have++] = l;
                    else {
                        if (16 === l) {
                            for(T = I + 2; g < T;){
                                if (0 === E) break A;
                                E--, r += i[a++] << g, g += 8;
                            }
                            if (r >>>= I, g -= I, 0 === e.have) {
                                A.msg = "invalid bit length repeat", e.mode = $ec3de3bad43fb783$var$Pt;
                                break;
                            }
                            R = e.lens[e.have - 1], w = 3 + (3 & r), r >>>= 2, g -= 2;
                        } else if (17 === l) {
                            for(T = I + 3; g < T;){
                                if (0 === E) break A;
                                E--, r += i[a++] << g, g += 8;
                            }
                            r >>>= I, g -= I, R = 0, w = 3 + (7 & r), r >>>= 3, g -= 3;
                        } else {
                            for(T = I + 7; g < T;){
                                if (0 === E) break A;
                                E--, r += i[a++] << g, g += 8;
                            }
                            r >>>= I, g -= I, R = 0, w = 11 + (127 & r), r >>>= 7, g -= 7;
                        }
                        if (e.have + w > e.nlen + e.ndist) {
                            A.msg = "invalid bit length repeat", e.mode = $ec3de3bad43fb783$var$Pt;
                            break;
                        }
                        for(; w--;)e.lens[e.have++] = R;
                    }
                }
                if (e.mode === $ec3de3bad43fb783$var$Pt) break;
                if (0 === e.lens[256]) {
                    A.msg = "invalid code -- missing end-of-block", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if (e.lenbits = 9, F = {
                    bits: e.lenbits
                }, S = $ec3de3bad43fb783$var$wt(1, e.lens, 0, e.nlen, e.lencode, 0, e.work, F), e.lenbits = F.bits, S) {
                    A.msg = "invalid literal/lengths set", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if (e.distbits = 6, e.distcode = e.distdyn, F = {
                    bits: e.distbits
                }, S = $ec3de3bad43fb783$var$wt(2, e.lens, e.nlen, e.ndist, e.distcode, 0, e.work, F), e.distbits = F.bits, S) {
                    A.msg = "invalid distances set", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                if (e.mode = $ec3de3bad43fb783$var$yt, t === $ec3de3bad43fb783$var$It) break A;
            case $ec3de3bad43fb783$var$yt:
                e.mode = $ec3de3bad43fb783$var$kt;
            case $ec3de3bad43fb783$var$kt:
                if (E >= 6 && h >= 258) {
                    A.next_out = n, A.avail_out = h, A.next_in = a, A.avail_in = E, e.hold = r, e.bits = g, $ec3de3bad43fb783$var$Et(A, B), n = A.next_out, s = A.output, h = A.avail_out, a = A.next_in, i = A.input, E = A.avail_in, r = e.hold, g = e.bits, e.mode === $ec3de3bad43fb783$var$Tt && (e.back = -1);
                    break;
                }
                for(e.back = 0; Q = e.lencode[r & (1 << e.lenbits) - 1], I = Q >>> 24, _ = Q >>> 16 & 255, l = 65535 & Q, !(I <= g);){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                if (_ && 0 == (240 & _)) {
                    for(d = I, M = _, D = l; Q = e.lencode[D + ((r & (1 << d + M) - 1) >> d)], I = Q >>> 24, _ = Q >>> 16 & 255, l = 65535 & Q, !(d + I <= g);){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    r >>>= d, g -= d, e.back += d;
                }
                if (r >>>= I, g -= I, e.back += I, e.length = l, 0 === _) {
                    e.mode = 16205;
                    break;
                }
                if (32 & _) {
                    e.back = -1, e.mode = $ec3de3bad43fb783$var$Tt;
                    break;
                }
                if (64 & _) {
                    A.msg = "invalid literal/length code", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.extra = 15 & _, e.mode = 16201;
            case 16201:
                if (e.extra) {
                    for(T = e.extra; g < T;){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    e.length += r & (1 << e.extra) - 1, r >>>= e.extra, g -= e.extra, e.back += e.extra;
                }
                e.was = e.length, e.mode = 16202;
            case 16202:
                for(; Q = e.distcode[r & (1 << e.distbits) - 1], I = Q >>> 24, _ = Q >>> 16 & 255, l = 65535 & Q, !(I <= g);){
                    if (0 === E) break A;
                    E--, r += i[a++] << g, g += 8;
                }
                if (0 == (240 & _)) {
                    for(d = I, M = _, D = l; Q = e.distcode[D + ((r & (1 << d + M) - 1) >> d)], I = Q >>> 24, _ = Q >>> 16 & 255, l = 65535 & Q, !(d + I <= g);){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    r >>>= d, g -= d, e.back += d;
                }
                if (r >>>= I, g -= I, e.back += I, 64 & _) {
                    A.msg = "invalid distance code", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.offset = l, e.extra = 15 & _, e.mode = 16203;
            case 16203:
                if (e.extra) {
                    for(T = e.extra; g < T;){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    e.offset += r & (1 << e.extra) - 1, r >>>= e.extra, g -= e.extra, e.back += e.extra;
                }
                if (e.offset > e.dmax) {
                    A.msg = "invalid distance too far back", e.mode = $ec3de3bad43fb783$var$Pt;
                    break;
                }
                e.mode = 16204;
            case 16204:
                if (0 === h) break A;
                if (w = B - h, e.offset > w) {
                    if (w = e.offset - w, w > e.whave && e.sane) {
                        A.msg = "invalid distance too far back", e.mode = $ec3de3bad43fb783$var$Pt;
                        break;
                    }
                    w > e.wnext ? (w -= e.wnext, c = e.wsize - w) : c = e.wnext - w, w > e.length && (w = e.length), C = e.window;
                } else C = s, c = n - e.offset, w = e.length;
                w > h && (w = h), h -= w, e.length -= w;
                do s[n++] = C[c++];
                while (--w);
                0 === e.length && (e.mode = $ec3de3bad43fb783$var$kt);
                break;
            case 16205:
                if (0 === h) break A;
                s[n++] = e.length, h--, e.mode = $ec3de3bad43fb783$var$kt;
                break;
            case $ec3de3bad43fb783$var$Ht:
                if (e.wrap) {
                    for(; g < 32;){
                        if (0 === E) break A;
                        E--, r |= i[a++] << g, g += 8;
                    }
                    if (B -= h, A.total_out += B, e.total += B, 4 & e.wrap && B && (A.adler = e.check = e.flags ? $ec3de3bad43fb783$var$x(e.check, s, B, n - B) : $ec3de3bad43fb783$var$b(e.check, s, B, n - B)), B = h, 4 & e.wrap && (e.flags ? r : $ec3de3bad43fb783$var$Ot(r)) !== e.check) {
                        A.msg = "incorrect data check", e.mode = $ec3de3bad43fb783$var$Pt;
                        break;
                    }
                    r = 0, g = 0;
                }
                e.mode = 16207;
            case 16207:
                if (e.wrap && e.flags) {
                    for(; g < 32;){
                        if (0 === E) break A;
                        E--, r += i[a++] << g, g += 8;
                    }
                    if (4 & e.wrap && r !== (4294967295 & e.total)) {
                        A.msg = "incorrect length check", e.mode = $ec3de3bad43fb783$var$Pt;
                        break;
                    }
                    r = 0, g = 0;
                }
                e.mode = 16208;
            case 16208:
                S = $ec3de3bad43fb783$var$lt;
                break A;
            case $ec3de3bad43fb783$var$Pt:
                S = $ec3de3bad43fb783$var$Dt;
                break A;
            case 16210:
                return $ec3de3bad43fb783$var$Rt;
            default:
                return $ec3de3bad43fb783$var$Mt;
        }
        return A.next_out = n, A.avail_out = h, A.next_in = a, A.avail_in = E, e.hold = r, e.bits = g, (e.wsize || B !== A.avail_out && e.mode < $ec3de3bad43fb783$var$Pt && (e.mode < $ec3de3bad43fb783$var$Ht || t !== $ec3de3bad43fb783$var$ct)) && $ec3de3bad43fb783$var$Nt(A, A.output, A.next_out, B - A.avail_out), o -= A.avail_in, B -= A.avail_out, A.total_in += o, A.total_out += B, e.total += B, 4 & e.wrap && B && (A.adler = e.check = e.flags ? $ec3de3bad43fb783$var$x(e.check, s, B, A.next_out - B) : $ec3de3bad43fb783$var$b(e.check, s, B, A.next_out - B)), A.data_type = e.bits + (e.last ? 64 : 0) + (e.mode === $ec3de3bad43fb783$var$Tt ? 128 : 0) + (e.mode === $ec3de3bad43fb783$var$yt || e.mode === $ec3de3bad43fb783$var$pt ? 256 : 0), (0 === o && 0 === B || t === $ec3de3bad43fb783$var$ct) && S === $ec3de3bad43fb783$var$_t && (S = $ec3de3bad43fb783$var$St), S;
    },
    inflateEnd: (A)=>{
        if ($ec3de3bad43fb783$var$Gt(A)) return $ec3de3bad43fb783$var$Mt;
        let t = A.state;
        return t.window && (t.window = null), A.state = null, $ec3de3bad43fb783$var$_t;
    },
    inflateGetHeader: (A, t)=>{
        if ($ec3de3bad43fb783$var$Gt(A)) return $ec3de3bad43fb783$var$Mt;
        const e = A.state;
        return 0 == (2 & e.wrap) ? $ec3de3bad43fb783$var$Mt : (e.head = t, t.done = !1, $ec3de3bad43fb783$var$_t);
    },
    inflateSetDictionary: (A, t)=>{
        const e = t.length;
        let i, s, a;
        return $ec3de3bad43fb783$var$Gt(A) ? $ec3de3bad43fb783$var$Mt : (i = A.state, 0 !== i.wrap && i.mode !== $ec3de3bad43fb783$var$Ft ? $ec3de3bad43fb783$var$Mt : i.mode === $ec3de3bad43fb783$var$Ft && (s = 1, s = $ec3de3bad43fb783$var$b(s, t, e, 0), s !== i.check) ? $ec3de3bad43fb783$var$Dt : (a = $ec3de3bad43fb783$var$Nt(A, t, e, e), a ? (i.mode = 16210, $ec3de3bad43fb783$var$Rt) : (i.havedict = 1, $ec3de3bad43fb783$var$_t)));
    },
    inflateInfo: "pako inflate (from Nodeca project)"
};
var $ec3de3bad43fb783$var$jt = function() {
    this.text = 0, this.time = 0, this.xflags = 0, this.os = 0, this.extra = null, this.extra_len = 0, this.name = "", this.comment = "", this.hcrc = 0, this.done = !1;
};
const $ec3de3bad43fb783$var$Zt = Object.prototype.toString, { Z_NO_FLUSH: $ec3de3bad43fb783$var$Wt, Z_FINISH: $ec3de3bad43fb783$var$Xt, Z_OK: $ec3de3bad43fb783$var$qt, Z_STREAM_END: $ec3de3bad43fb783$var$Vt, Z_NEED_DICT: $ec3de3bad43fb783$var$$t, Z_STREAM_ERROR: $ec3de3bad43fb783$var$Ae, Z_DATA_ERROR: $ec3de3bad43fb783$var$te, Z_MEM_ERROR: $ec3de3bad43fb783$var$ee } = $ec3de3bad43fb783$var$J;
function $ec3de3bad43fb783$var$ie(A) {
    this.options = $ec3de3bad43fb783$var$LA.assign({
        chunkSize: 65536,
        windowBits: 15,
        to: ""
    }, A || {});
    const t = this.options;
    t.raw && t.windowBits >= 0 && t.windowBits < 16 && (t.windowBits = -t.windowBits, 0 === t.windowBits && (t.windowBits = -15)), !(t.windowBits >= 0 && t.windowBits < 16) || A && A.windowBits || (t.windowBits += 32), t.windowBits > 15 && t.windowBits < 48 && 0 == (15 & t.windowBits) && (t.windowBits |= 15), this.err = 0, this.msg = "", this.ended = !1, this.chunks = [], this.strm = new $ec3de3bad43fb783$var$vA, this.strm.avail_out = 0;
    let e = $ec3de3bad43fb783$var$vt.inflateInit2(this.strm, t.windowBits);
    if (e !== $ec3de3bad43fb783$var$qt) throw new Error($ec3de3bad43fb783$var$L[e]);
    if (this.header = new $ec3de3bad43fb783$var$jt, $ec3de3bad43fb783$var$vt.inflateGetHeader(this.strm, this.header), t.dictionary && ("string" == typeof t.dictionary ? t.dictionary = $ec3de3bad43fb783$var$NA.string2buf(t.dictionary) : "[object ArrayBuffer]" === $ec3de3bad43fb783$var$Zt.call(t.dictionary) && (t.dictionary = new Uint8Array(t.dictionary)), t.raw && (e = $ec3de3bad43fb783$var$vt.inflateSetDictionary(this.strm, t.dictionary), e !== $ec3de3bad43fb783$var$qt))) throw new Error($ec3de3bad43fb783$var$L[e]);
}
function $ec3de3bad43fb783$var$se(A, t) {
    const e = new $ec3de3bad43fb783$var$ie(t);
    if (e.push(A), e.err) throw e.msg || $ec3de3bad43fb783$var$L[e.err];
    return e.result;
}
$ec3de3bad43fb783$var$ie.prototype.push = function(A, t) {
    const e = this.strm, i = this.options.chunkSize, s = this.options.dictionary;
    let a, n, E;
    if (this.ended) return !1;
    for(n = t === ~~t ? t : !0 === t ? $ec3de3bad43fb783$var$Xt : $ec3de3bad43fb783$var$Wt, "[object ArrayBuffer]" === $ec3de3bad43fb783$var$Zt.call(A) ? e.input = new Uint8Array(A) : e.input = A, e.next_in = 0, e.avail_in = e.input.length;;){
        for(0 === e.avail_out && (e.output = new Uint8Array(i), e.next_out = 0, e.avail_out = i), a = $ec3de3bad43fb783$var$vt.inflate(e, n), a === $ec3de3bad43fb783$var$$t && s && (a = $ec3de3bad43fb783$var$vt.inflateSetDictionary(e, s), a === $ec3de3bad43fb783$var$qt ? a = $ec3de3bad43fb783$var$vt.inflate(e, n) : a === $ec3de3bad43fb783$var$te && (a = $ec3de3bad43fb783$var$$t)); e.avail_in > 0 && a === $ec3de3bad43fb783$var$Vt && e.state.wrap > 0 && 0 !== A[e.next_in];)$ec3de3bad43fb783$var$vt.inflateReset(e), a = $ec3de3bad43fb783$var$vt.inflate(e, n);
        switch(a){
            case $ec3de3bad43fb783$var$Ae:
            case $ec3de3bad43fb783$var$te:
            case $ec3de3bad43fb783$var$$t:
            case $ec3de3bad43fb783$var$ee:
                return this.onEnd(a), this.ended = !0, !1;
        }
        if (E = e.avail_out, e.next_out && (0 === e.avail_out || a === $ec3de3bad43fb783$var$Vt)) {
            if ("string" === this.options.to) {
                let A = $ec3de3bad43fb783$var$NA.utf8border(e.output, e.next_out), t = e.next_out - A, s = $ec3de3bad43fb783$var$NA.buf2string(e.output, A);
                e.next_out = t, e.avail_out = i - t, t && e.output.set(e.output.subarray(A, A + t), 0), this.onData(s);
            } else this.onData(e.output.length === e.next_out ? e.output : e.output.subarray(0, e.next_out));
        }
        if (a !== $ec3de3bad43fb783$var$qt || 0 !== E) {
            if (a === $ec3de3bad43fb783$var$Vt) return a = $ec3de3bad43fb783$var$vt.inflateEnd(this.strm), this.onEnd(a), this.ended = !0, !0;
            if (0 === e.avail_in) break;
        }
    }
    return !0;
}, $ec3de3bad43fb783$var$ie.prototype.onData = function(A) {
    this.chunks.push(A);
}, $ec3de3bad43fb783$var$ie.prototype.onEnd = function(A) {
    A === $ec3de3bad43fb783$var$qt && ("string" === this.options.to ? this.result = this.chunks.join("") : this.result = $ec3de3bad43fb783$var$LA.flattenChunks(this.chunks)), this.chunks = [], this.err = A, this.msg = this.strm.msg;
};
var $ec3de3bad43fb783$var$ae = {
    Inflate: $ec3de3bad43fb783$var$ie,
    inflate: $ec3de3bad43fb783$var$se,
    inflateRaw: function(A, t) {
        return (t = t || {}).raw = !0, $ec3de3bad43fb783$var$se(A, t);
    },
    ungzip: $ec3de3bad43fb783$var$se,
    constants: $ec3de3bad43fb783$var$J
};
const { Deflate: $ec3de3bad43fb783$var$ne, deflate: $ec3de3bad43fb783$var$Ee, deflateRaw: $ec3de3bad43fb783$var$he, gzip: $ec3de3bad43fb783$var$re } = $ec3de3bad43fb783$var$at, { Inflate: $ec3de3bad43fb783$var$ge, inflate: $ec3de3bad43fb783$var$oe, inflateRaw: $ec3de3bad43fb783$var$Be, ungzip: $ec3de3bad43fb783$var$we } = $ec3de3bad43fb783$var$ae;
var $ec3de3bad43fb783$var$ce = $ec3de3bad43fb783$var$Ee, $ec3de3bad43fb783$var$Ce = $ec3de3bad43fb783$var$ge;
class $ec3de3bad43fb783$export$86495b081fef8e52 {
    constructor(A, t = !1, e = !0){
        this.device = A, this.tracing = t, this.slipReaderEnabled = !1, this.leftOver = new Uint8Array(0), this.baudrate = 0, this.traceLog = "", this.lastTraceTime = Date.now(), this._DTR_state = !1, this.slipReaderEnabled = e;
    }
    getInfo() {
        const A = this.device.getInfo();
        return A.usbVendorId && A.usbProductId ? `WebSerial VendorID 0x${A.usbVendorId.toString(16)} ProductID 0x${A.usbProductId.toString(16)}` : "";
    }
    getPid() {
        return this.device.getInfo().usbProductId;
    }
    trace(A) {
        const t = `${`TRACE ${(Date.now() - this.lastTraceTime).toFixed(3)}`} ${A}`;
        console.log(t), this.traceLog += t + "\n";
    }
    async returnTrace() {
        try {
            await navigator.clipboard.writeText(this.traceLog), console.log("Text copied to clipboard!");
        } catch (A) {
            console.error("Failed to copy text:", A);
        }
    }
    hexify(A) {
        return Array.from(A).map((A)=>A.toString(16).padStart(2, "0")).join("").padEnd(16, " ");
    }
    hexConvert(A, t = !0) {
        if (t && A.length > 16) {
            let t = "", e = A;
            for(; e.length > 0;){
                const A = e.slice(0, 16), i = String.fromCharCode(...A).split("").map((A)=>" " === A || A >= " " && A <= "~" && "  " !== A ? A : ".").join("");
                e = e.slice(16), t += `\n    ${this.hexify(A.slice(0, 8))} ${this.hexify(A.slice(8))} | ${i}`;
            }
            return t;
        }
        return this.hexify(A);
    }
    slipWriter(A) {
        const t = [];
        t.push(192);
        for(let e = 0; e < A.length; e++)219 === A[e] ? t.push(219, 221) : 192 === A[e] ? t.push(219, 220) : t.push(A[e]);
        return t.push(192), new Uint8Array(t);
    }
    async write(A) {
        const t = this.slipWriter(A);
        if (this.device.writable) {
            const A = this.device.writable.getWriter();
            this.tracing && (console.log("Write bytes"), this.trace(`Write ${t.length} bytes: ${this.hexConvert(t)}`)), await A.write(t), A.releaseLock();
        }
    }
    _appendBuffer(A, t) {
        const e = new Uint8Array(A.byteLength + t.byteLength);
        return e.set(new Uint8Array(A), 0), e.set(new Uint8Array(t), A.byteLength), e.buffer;
    }
    slipReader(A) {
        let t = 0, e = 0, i = 0, s = "init";
        for(; t < A.length;)if ("init" !== s || 192 != A[t]) {
            if ("valid_data" === s && 192 == A[t]) {
                i = t - 1, s = "packet_complete";
                break;
            }
            t++;
        } else e = t + 1, s = "valid_data", t++;
        if ("packet_complete" !== s) return this.leftOver = A, new Uint8Array(0);
        this.leftOver = A.slice(i + 2);
        const a = new Uint8Array(i - e + 1);
        let n = 0;
        for(t = e; t <= i; t++, n++)219 !== A[t] || 220 !== A[t + 1] ? 219 !== A[t] || 221 !== A[t + 1] ? a[n] = A[t] : (a[n] = 219, t++) : (a[n] = 192, t++);
        return a.slice(0, n);
    }
    async read(A = 0, t = 12) {
        let e, i = this.leftOver;
        if (this.leftOver = new Uint8Array(0), this.slipReaderEnabled) {
            const A = this.slipReader(i);
            if (A.length > 0) return A;
            i = this.leftOver, this.leftOver = new Uint8Array(0);
        }
        if (null == this.device.readable) return this.leftOver;
        this.reader = this.device.readable.getReader();
        try {
            A > 0 && (e = setTimeout(()=>{
                this.reader && this.reader.cancel();
            }, A));
            do {
                const { value: A, done: t } = await this.reader.read();
                if (t) throw this.leftOver = i, new Error("Timeout");
                i = new Uint8Array(this._appendBuffer(i.buffer, A.buffer));
            }while (i.length < t);
        } finally{
            A > 0 && clearTimeout(e), this.reader.releaseLock();
        }
        if (this.tracing && (console.log("Read bytes"), this.trace(`Read ${i.length} bytes: ${this.hexConvert(i)}`)), this.slipReaderEnabled) {
            const A = this.slipReader(i);
            return this.tracing && (console.log("Slip reader results"), this.trace(`Read ${A.length} bytes: ${this.hexConvert(A)}`)), A;
        }
        return i;
    }
    async rawRead(A = 0) {
        if (0 != this.leftOver.length) {
            const A = this.leftOver;
            return this.leftOver = new Uint8Array(0), A;
        }
        if (!this.device.readable) return this.leftOver;
        let t;
        this.reader = this.device.readable.getReader();
        try {
            A > 0 && (t = setTimeout(()=>{
                this.reader && this.reader.cancel();
            }, A));
            const { value: e, done: i } = await this.reader.read();
            return i || this.tracing && (console.log("Raw Read bytes"), this.trace(`Read ${e.length} bytes: ${this.hexConvert(e)}`)), e;
        } finally{
            A > 0 && clearTimeout(t), this.reader.releaseLock();
        }
    }
    async setRTS(A) {
        await this.device.setSignals({
            requestToSend: A
        }), await this.setDTR(this._DTR_state);
    }
    async setDTR(A) {
        this._DTR_state = A, await this.device.setSignals({
            dataTerminalReady: A
        });
    }
    async connect(A = 115200, t = {}) {
        await this.device.open({
            baudRate: A,
            dataBits: null == t ? void 0 : t.dataBits,
            stopBits: null == t ? void 0 : t.stopBits,
            bufferSize: null == t ? void 0 : t.bufferSize,
            parity: null == t ? void 0 : t.parity,
            flowControl: null == t ? void 0 : t.flowControl
        }), this.baudrate = A, this.leftOver = new Uint8Array(0);
    }
    async sleep(A) {
        return new Promise((t)=>setTimeout(t, A));
    }
    async waitForUnlock(A) {
        for(; this.device.readable && this.device.readable.locked || this.device.writable && this.device.writable.locked;)await this.sleep(A);
    }
    async disconnect() {
        var A, t;
        (null === (A = this.device.readable) || void 0 === A ? void 0 : A.locked) && await (null === (t = this.reader) || void 0 === t ? void 0 : t.cancel()), await this.waitForUnlock(400), this.reader = void 0, await this.device.close();
    }
}
function $ec3de3bad43fb783$var$_e(A) {
    return new Promise((t)=>setTimeout(t, A));
}
async function $ec3de3bad43fb783$export$d3423661fbfd2b60(A, t = 50) {
    await A.setDTR(!1), await A.setRTS(!0), await $ec3de3bad43fb783$var$_e(100), await A.setDTR(!0), await A.setRTS(!1), await $ec3de3bad43fb783$var$_e(t), await A.setDTR(!1);
}
async function $ec3de3bad43fb783$export$3252bdf5627aa8a3(A) {
    await A.setRTS(!1), await A.setDTR(!1), await $ec3de3bad43fb783$var$_e(100), await A.setDTR(!0), await A.setRTS(!1), await $ec3de3bad43fb783$var$_e(100), await A.setRTS(!0), await A.setDTR(!1), await A.setRTS(!0), await $ec3de3bad43fb783$var$_e(100), await A.setRTS(!1), await A.setDTR(!1);
}
async function $ec3de3bad43fb783$export$4ff35e5b1175d6f6(A, t = !1) {
    t ? (await $ec3de3bad43fb783$var$_e(200), await A.setRTS(!1), await $ec3de3bad43fb783$var$_e(200)) : (await $ec3de3bad43fb783$var$_e(100), await A.setRTS(!1));
}
function $ec3de3bad43fb783$export$929a22f56823f4cb(A) {
    const t = [
        "D",
        "R",
        "W"
    ], e = A.split("|");
    for (const A of e){
        const e = A[0], i = A.slice(1);
        if (!t.includes(e)) return !1;
        if ("D" === e || "R" === e) {
            if ("0" !== i && "1" !== i) return !1;
        } else if ("W" === e) {
            const A = parseInt(i);
            if (isNaN(A) || A <= 0) return !1;
        }
    }
    return !0;
}
async function $ec3de3bad43fb783$export$e5e6796b349bcc84(A, t) {
    const e = {
        D: async (t)=>await A.setDTR(t),
        R: async (t)=>await A.setRTS(t),
        W: async (A)=>await $ec3de3bad43fb783$var$_e(A)
    };
    try {
        if (!$ec3de3bad43fb783$export$929a22f56823f4cb(t)) return;
        const A = t.split("|");
        for (const t of A){
            const A = t[0], i = t.slice(1);
            "W" === A ? await e.W(Number(i)) : "D" !== A && "R" !== A || await e[A]("1" === i);
        }
    } catch (A) {
        throw new Error("Invalid custom reset sequence");
    }
}
var $ec3de3bad43fb783$var$Se = function(A) {
    return atob(A);
};
class $ec3de3bad43fb783$export$b0f7a6c745790308 {
    constructor(A){
        this.ESP_RAM_BLOCK = 6144, this.ESP_FLASH_BEGIN = 2, this.ESP_FLASH_DATA = 3, this.ESP_FLASH_END = 4, this.ESP_MEM_BEGIN = 5, this.ESP_MEM_END = 6, this.ESP_MEM_DATA = 7, this.ESP_WRITE_REG = 9, this.ESP_READ_REG = 10, this.ESP_SPI_ATTACH = 13, this.ESP_CHANGE_BAUDRATE = 15, this.ESP_FLASH_DEFL_BEGIN = 16, this.ESP_FLASH_DEFL_DATA = 17, this.ESP_FLASH_DEFL_END = 18, this.ESP_SPI_FLASH_MD5 = 19, this.ESP_ERASE_FLASH = 208, this.ESP_ERASE_REGION = 209, this.ESP_READ_FLASH = 210, this.ESP_RUN_USER_CODE = 211, this.ESP_IMAGE_MAGIC = 233, this.ESP_CHECKSUM_MAGIC = 239, this.ROM_INVALID_RECV_MSG = 5, this.ERASE_REGION_TIMEOUT_PER_MB = 3e4, this.ERASE_WRITE_TIMEOUT_PER_MB = 4e4, this.MD5_TIMEOUT_PER_MB = 8e3, this.CHIP_ERASE_TIMEOUT = 12e4, this.FLASH_READ_TIMEOUT = 1e5, this.MAX_TIMEOUT = 2 * this.CHIP_ERASE_TIMEOUT, this.CHIP_DETECT_MAGIC_REG_ADDR = 1073745920, this.DETECTED_FLASH_SIZES = {
            18: "256KB",
            19: "512KB",
            20: "1MB",
            21: "2MB",
            22: "4MB",
            23: "8MB",
            24: "16MB"
        }, this.DETECTED_FLASH_SIZES_NUM = {
            18: 256,
            19: 512,
            20: 1024,
            21: 2048,
            22: 4096,
            23: 8192,
            24: 16384
        }, this.USB_JTAG_SERIAL_PID = 4097, this.romBaudrate = 115200, this.debugLogging = !1, this.syncStubDetected = !1, this.checksum = function(A) {
            let t, e = 239;
            for(t = 0; t < A.length; t++)e ^= A[t];
            return e;
        }, this.timeoutPerMb = function(A, t) {
            const e = A * (t / 1e6);
            return e < 3e3 ? 3e3 : e;
        }, this.flashSizeBytes = function(A) {
            let t = -1;
            return -1 !== A.indexOf("KB") ? t = 1024 * parseInt(A.slice(0, A.indexOf("KB"))) : -1 !== A.indexOf("MB") && (t = 1024 * parseInt(A.slice(0, A.indexOf("MB"))) * 1024), t;
        }, this.IS_STUB = !1, this.FLASH_WRITE_SIZE = 16384, this.transport = A.transport, this.baudrate = A.baudrate, A.serialOptions && (this.serialOptions = A.serialOptions), A.romBaudrate && (this.romBaudrate = A.romBaudrate), A.terminal && (this.terminal = A.terminal, this.terminal.clean()), void 0 !== A.debugLogging && (this.debugLogging = A.debugLogging), A.port && (this.transport = new $ec3de3bad43fb783$export$86495b081fef8e52(A.port)), void 0 !== A.enableTracing && (this.transport.tracing = A.enableTracing), this.info("esptool.js"), this.info("Serial port " + this.transport.getInfo());
    }
    _sleep(A) {
        return new Promise((t)=>setTimeout(t, A));
    }
    write(A, t = !0) {
        this.terminal ? t ? this.terminal.writeLine(A) : this.terminal.write(A) : console.log(A);
    }
    error(A, t = !0) {
        this.write(`Error: ${A}`, t);
    }
    info(A, t = !0) {
        this.write(A, t);
    }
    debug(A, t = !0) {
        this.debugLogging && this.write(`Debug: ${A}`, t);
    }
    _shortToBytearray(A) {
        return new Uint8Array([
            255 & A,
            A >> 8 & 255
        ]);
    }
    _intToByteArray(A) {
        return new Uint8Array([
            255 & A,
            A >> 8 & 255,
            A >> 16 & 255,
            A >> 24 & 255
        ]);
    }
    _byteArrayToShort(A, t) {
        return A | t >> 8;
    }
    _byteArrayToInt(A, t, e, i) {
        return A | t << 8 | e << 16 | i << 24;
    }
    _appendBuffer(A, t) {
        const e = new Uint8Array(A.byteLength + t.byteLength);
        return e.set(new Uint8Array(A), 0), e.set(new Uint8Array(t), A.byteLength), e.buffer;
    }
    _appendArray(A, t) {
        const e = new Uint8Array(A.length + t.length);
        return e.set(A, 0), e.set(t, A.length), e;
    }
    ui8ToBstr(A) {
        let t = "";
        for(let e = 0; e < A.length; e++)t += String.fromCharCode(A[e]);
        return t;
    }
    bstrToUi8(A) {
        const t = new Uint8Array(A.length);
        for(let e = 0; e < A.length; e++)t[e] = A.charCodeAt(e);
        return t;
    }
    async flushInput() {
        try {
            await this.transport.rawRead(200);
        } catch (A) {
            this.error(A.message);
        }
    }
    async readPacket(t = null, e = 3e3) {
        for(let i = 0; i < 100; i++){
            const i = await this.transport.read(e), s = i[0], a = i[1], n = this._byteArrayToInt(i[4], i[5], i[6], i[7]), E = i.slice(8);
            if (1 == s) {
                if (null == t || a == t) return [
                    n,
                    E
                ];
                if (0 != E[0] && E[1] == this.ROM_INVALID_RECV_MSG) throw await this.flushInput(), new $ec3de3bad43fb783$var$A("unsupported command error");
            }
        }
        throw new $ec3de3bad43fb783$var$A("invalid response");
    }
    async command(A = null, t = new Uint8Array(0), e = 0, i = !0, s = 3e3) {
        if (null != A) {
            this.transport.tracing && this.transport.trace(`command op:0x${A.toString(16).padStart(2, "0")} data len=${t.length} wait_response=${i ? 1 : 0} timeout=${(s / 1e3).toFixed(3)} data=${this.transport.hexConvert(t)}`);
            const a = new Uint8Array(8 + t.length);
            let n;
            for(a[0] = 0, a[1] = A, a[2] = this._shortToBytearray(t.length)[0], a[3] = this._shortToBytearray(t.length)[1], a[4] = this._intToByteArray(e)[0], a[5] = this._intToByteArray(e)[1], a[6] = this._intToByteArray(e)[2], a[7] = this._intToByteArray(e)[3], n = 0; n < t.length; n++)a[8 + n] = t[n];
            await this.transport.write(a);
        }
        return i ? this.readPacket(A, s) : [
            0,
            new Uint8Array(0)
        ];
    }
    async readReg(A, t = 3e3) {
        const e = this._intToByteArray(A);
        return (await this.command(this.ESP_READ_REG, e, void 0, void 0, t))[0];
    }
    async writeReg(A, t, e = 4294967295, i = 0, s = 0) {
        let a = this._appendArray(this._intToByteArray(A), this._intToByteArray(t));
        a = this._appendArray(a, this._intToByteArray(e)), a = this._appendArray(a, this._intToByteArray(i)), s > 0 && (a = this._appendArray(a, this._intToByteArray(this.chip.UART_DATE_REG_ADDR)), a = this._appendArray(a, this._intToByteArray(0)), a = this._appendArray(a, this._intToByteArray(0)), a = this._appendArray(a, this._intToByteArray(s))), await this.checkCommand("write target memory", this.ESP_WRITE_REG, a);
    }
    async sync() {
        this.debug("Sync");
        const A = new Uint8Array(36);
        let t;
        for(A[0] = 7, A[1] = 7, A[2] = 18, A[3] = 32, t = 0; t < 32; t++)A[4 + t] = 85;
        try {
            const t = await this.command(8, A, void 0, void 0, 100);
            return this.syncStubDetected = this.syncStubDetected && 0 === t[0], t;
        } catch (A) {
            throw this.debug("Sync err " + A), A;
        }
    }
    async _connectAttempt(A = "default_reset", t = !1) {
        if (this.debug("_connect_attempt " + A + " " + t), "no_reset" !== A) {
            if (this.transport.getPid() === this.USB_JTAG_SERIAL_PID) await $ec3de3bad43fb783$export$3252bdf5627aa8a3(this.transport);
            else {
                const A = t ? "D0|R1|W100|W2000|D1|R0|W50|D0" : "D0|R1|W100|D1|R0|W50|D0";
                await $ec3de3bad43fb783$export$e5e6796b349bcc84(this.transport, A);
            }
        }
        let e = 0, i = !0;
        for(; i;){
            try {
                e += (await this.transport.read(1e3)).length;
            } catch (A) {
                if (this.debug(A.message), A instanceof Error) {
                    i = !1;
                    break;
                }
            }
            await this._sleep(50);
        }
        for(this.transport.slipReaderEnabled = !0, this.syncStubDetected = !0, e = 7; e--;){
            try {
                const A = await this.sync();
                return this.debug(A[0].toString()), "success";
            } catch (A) {
                A instanceof Error && (t ? this.info("_", !1) : this.info(".", !1));
            }
            await this._sleep(50);
        }
        return "error";
    }
    async connect(t = "default_reset", e = 7, i = !1) {
        let s, a;
        for(this.info("Connecting...", !1), await this.transport.connect(this.romBaudrate, this.serialOptions), s = 0; s < e && (a = await this._connectAttempt(t, !1), "success" !== a) && (a = await this._connectAttempt(t, !0), "success" !== a); s++);
        if ("success" !== a) throw new $ec3de3bad43fb783$var$A("Failed to connect with the device");
        if (this.info("\n\r", !1), !i) {
            const t = await this.readReg(1073745920) >>> 0;
            this.debug("Chip Magic " + t.toString(16));
            const e = await async function(A) {
                switch(A){
                    case 15736195:
                        {
                            const { ESP32ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$He;
                            });
                            return new A;
                        }
                    case 1867591791:
                    case 2084675695:
                        {
                            const { ESP32C2ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$Ne;
                            });
                            return new A;
                        }
                    case 1763790959:
                    case 456216687:
                    case 1216438383:
                    case 1130455151:
                        {
                            const { ESP32C3ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$be;
                            });
                            return new A;
                        }
                    case 752910447:
                        {
                            const { ESP32C6ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$Ve;
                            });
                            return new A;
                        }
                    case 285294703:
                        {
                            const { ESP32C5ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$si;
                            });
                            return new A;
                        }
                    case 3619110528:
                        {
                            const { ESP32H2ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$gi;
                            });
                            return new A;
                        }
                    case 9:
                        {
                            const { ESP32S3ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$Ii;
                            });
                            return new A;
                        }
                    case 1990:
                        {
                            const { ESP32S2ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$Ri;
                            });
                            return new A;
                        }
                    case 4293968129:
                        {
                            const { ESP8266ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$ui;
                            });
                            return new A;
                        }
                    case 0:
                    case 182303440:
                    case 117676761:
                        {
                            const { ESP32P4ROM: A } = await Promise.resolve().then(function() {
                                return $ec3de3bad43fb783$var$Oi;
                            });
                            return new A;
                        }
                    default:
                        return null;
                }
            }(t);
            if (null === this.chip) throw new $ec3de3bad43fb783$var$A(`Unexpected CHIP magic value ${t}. Failed to autodetect chip type.`);
            this.chip = e;
        }
    }
    async detectChip(A = "default_reset") {
        await this.connect(A), this.info("Detecting chip type... ", !1), null != this.chip ? this.info(this.chip.CHIP_NAME) : this.info("unknown!");
    }
    async checkCommand(A = "", t = null, e = new Uint8Array(0), i = 0, s = 3e3) {
        this.debug("check_command " + A);
        const a = await this.command(t, e, i, void 0, s);
        return a[1].length > 4 ? a[1] : a[0];
    }
    async memBegin(A, t, e, i) {
        this.debug("mem_begin " + A + " " + t + " " + e + " " + i.toString(16));
        let s = this._appendArray(this._intToByteArray(A), this._intToByteArray(t));
        s = this._appendArray(s, this._intToByteArray(e)), s = this._appendArray(s, this._intToByteArray(i)), await this.checkCommand("enter RAM download mode", this.ESP_MEM_BEGIN, s);
    }
    async memBlock(A, t) {
        let e = this._appendArray(this._intToByteArray(A.length), this._intToByteArray(t));
        e = this._appendArray(e, this._intToByteArray(0)), e = this._appendArray(e, this._intToByteArray(0)), e = this._appendArray(e, A);
        const i = this.checksum(A);
        await this.checkCommand("write to target RAM", this.ESP_MEM_DATA, e, i);
    }
    async memFinish(A) {
        const t = 0 === A ? 1 : 0, e = this._appendArray(this._intToByteArray(t), this._intToByteArray(A));
        await this.checkCommand("leave RAM download mode", this.ESP_MEM_END, e, void 0, 50);
    }
    async flashSpiAttach(A) {
        const t = this._intToByteArray(A);
        await this.checkCommand("configure SPI flash pins", this.ESP_SPI_ATTACH, t);
    }
    async flashBegin(A, t) {
        const e = Math.floor((A + this.FLASH_WRITE_SIZE - 1) / this.FLASH_WRITE_SIZE), i = this.chip.getEraseSize(t, A), s = new Date, a = s.getTime();
        let n = 3e3;
        0 == this.IS_STUB && (n = this.timeoutPerMb(this.ERASE_REGION_TIMEOUT_PER_MB, A)), this.debug("flash begin " + i + " " + e + " " + this.FLASH_WRITE_SIZE + " " + t + " " + A);
        let E = this._appendArray(this._intToByteArray(i), this._intToByteArray(e));
        E = this._appendArray(E, this._intToByteArray(this.FLASH_WRITE_SIZE)), E = this._appendArray(E, this._intToByteArray(t)), 0 == this.IS_STUB && (E = this._appendArray(E, this._intToByteArray(0))), await this.checkCommand("enter Flash download mode", this.ESP_FLASH_BEGIN, E, void 0, n);
        const h = s.getTime();
        return 0 != A && 0 == this.IS_STUB && this.info("Took " + (h - a) / 1e3 + "." + (h - a) % 1e3 + "s to erase flash block"), e;
    }
    async flashDeflBegin(A, t, e) {
        const i = Math.floor((t + this.FLASH_WRITE_SIZE - 1) / this.FLASH_WRITE_SIZE), s = Math.floor((A + this.FLASH_WRITE_SIZE - 1) / this.FLASH_WRITE_SIZE), a = new Date, n = a.getTime();
        let E, h;
        this.IS_STUB ? (E = A, h = 3e3) : (E = s * this.FLASH_WRITE_SIZE, h = this.timeoutPerMb(this.ERASE_REGION_TIMEOUT_PER_MB, E)), this.info("Compressed " + A + " bytes to " + t + "...");
        let r = this._appendArray(this._intToByteArray(E), this._intToByteArray(i));
        r = this._appendArray(r, this._intToByteArray(this.FLASH_WRITE_SIZE)), r = this._appendArray(r, this._intToByteArray(e)), "ESP32-S2" !== this.chip.CHIP_NAME && "ESP32-S3" !== this.chip.CHIP_NAME && "ESP32-C3" !== this.chip.CHIP_NAME && "ESP32-C2" !== this.chip.CHIP_NAME || !1 !== this.IS_STUB || (r = this._appendArray(r, this._intToByteArray(0))), await this.checkCommand("enter compressed flash mode", this.ESP_FLASH_DEFL_BEGIN, r, void 0, h);
        const g = a.getTime();
        return 0 != A && !1 === this.IS_STUB && this.info("Took " + (g - n) / 1e3 + "." + (g - n) % 1e3 + "s to erase flash block"), i;
    }
    async flashBlock(A, t, e) {
        let i = this._appendArray(this._intToByteArray(A.length), this._intToByteArray(t));
        i = this._appendArray(i, this._intToByteArray(0)), i = this._appendArray(i, this._intToByteArray(0)), i = this._appendArray(i, A);
        const s = this.checksum(A);
        await this.checkCommand("write to target Flash after seq " + t, this.ESP_FLASH_DATA, i, s, e);
    }
    async flashDeflBlock(A, t, e) {
        let i = this._appendArray(this._intToByteArray(A.length), this._intToByteArray(t));
        i = this._appendArray(i, this._intToByteArray(0)), i = this._appendArray(i, this._intToByteArray(0)), i = this._appendArray(i, A);
        const s = this.checksum(A);
        this.debug("flash_defl_block " + A[0].toString(16) + " " + A[1].toString(16)), await this.checkCommand("write compressed data to flash after seq " + t, this.ESP_FLASH_DEFL_DATA, i, s, e);
    }
    async flashFinish(A = !1) {
        const t = A ? 0 : 1, e = this._intToByteArray(t);
        await this.checkCommand("leave Flash mode", this.ESP_FLASH_END, e);
    }
    async flashDeflFinish(A = !1) {
        const t = A ? 0 : 1, e = this._intToByteArray(t);
        await this.checkCommand("leave compressed flash mode", this.ESP_FLASH_DEFL_END, e);
    }
    async runSpiflashCommand(t, e, i) {
        const s = this.chip.SPI_REG_BASE, a = s + 0, n = s + this.chip.SPI_USR_OFFS, E = s + this.chip.SPI_USR1_OFFS, h = s + this.chip.SPI_USR2_OFFS, r = s + this.chip.SPI_W0_OFFS;
        let g;
        g = null != this.chip.SPI_MOSI_DLEN_OFFS ? async (A, t)=>{
            const e = s + this.chip.SPI_MOSI_DLEN_OFFS, i = s + this.chip.SPI_MISO_DLEN_OFFS;
            A > 0 && await this.writeReg(e, A - 1), t > 0 && await this.writeReg(i, t - 1);
        } : async (A, t)=>{
            const e = E, i = (0 === t ? 0 : t - 1) << 8 | (0 === A ? 0 : A - 1) << 17;
            await this.writeReg(e, i);
        };
        const o = 262144;
        if (i > 32) throw new $ec3de3bad43fb783$var$A("Reading more than 32 bits back from a SPI flash operation is unsupported");
        if (e.length > 64) throw new $ec3de3bad43fb783$var$A("Writing more than 64 bytes of data with one SPI command is unsupported");
        const B = 8 * e.length, w = await this.readReg(n), c = await this.readReg(h);
        let C, I = -2147483648;
        i > 0 && (I |= 268435456), B > 0 && (I |= 134217728), await g(B, i), await this.writeReg(n, I);
        let _ = 1879048192 | t;
        if (await this.writeReg(h, _), 0 == B) await this.writeReg(r, 0);
        else {
            if (e.length % 4 != 0) {
                const A = new Uint8Array(e.length % 4);
                e = this._appendArray(e, A);
            }
            let A = r;
            for(C = 0; C < e.length - 4; C += 4)_ = this._byteArrayToInt(e[C], e[C + 1], e[C + 2], e[C + 3]), await this.writeReg(A, _), A += 4;
        }
        for(await this.writeReg(a, o), C = 0; C < 10 && (_ = await this.readReg(a) & o, 0 != _); C++);
        if (10 === C) throw new $ec3de3bad43fb783$var$A("SPI command did not complete in time");
        const l = await this.readReg(r);
        return await this.writeReg(n, w), await this.writeReg(h, c), l;
    }
    async readFlashId() {
        const A = new Uint8Array(0);
        return await this.runSpiflashCommand(159, A, 24);
    }
    async eraseFlash() {
        this.info("Erasing flash (this may take a while)...");
        let A = new Date;
        const t = A.getTime(), e = await this.checkCommand("erase flash", this.ESP_ERASE_FLASH, void 0, void 0, this.CHIP_ERASE_TIMEOUT);
        A = new Date;
        const i = A.getTime();
        return this.info("Chip erase completed successfully in " + (i - t) / 1e3 + "s"), e;
    }
    toHex(A) {
        return Array.prototype.map.call(A, (A)=>("00" + A.toString(16)).slice(-2)).join("");
    }
    async flashMd5sum(A, t) {
        const e = this.timeoutPerMb(this.MD5_TIMEOUT_PER_MB, t);
        let i = this._appendArray(this._intToByteArray(A), this._intToByteArray(t));
        i = this._appendArray(i, this._intToByteArray(0)), i = this._appendArray(i, this._intToByteArray(0));
        let s = await this.checkCommand("calculate md5sum", this.ESP_SPI_FLASH_MD5, i, void 0, e);
        s instanceof Uint8Array && s.length > 16 && (s = s.slice(0, 16));
        return this.toHex(s);
    }
    async readFlash(t, e, i = null) {
        let s = this._appendArray(this._intToByteArray(t), this._intToByteArray(e));
        s = this._appendArray(s, this._intToByteArray(4096)), s = this._appendArray(s, this._intToByteArray(1024));
        const a = await this.checkCommand("read flash", this.ESP_READ_FLASH, s);
        if (0 != a) throw new $ec3de3bad43fb783$var$A("Failed to read memory: " + a);
        let n = new Uint8Array(0);
        for(; n.length < e;){
            const t = await this.transport.read(this.FLASH_READ_TIMEOUT);
            if (!(t instanceof Uint8Array)) throw new $ec3de3bad43fb783$var$A("Failed to read memory: " + t);
            t.length > 0 && (n = this._appendArray(n, t), await this.transport.write(this._intToByteArray(n.length)), i && i(t, n.length, e));
        }
        return n;
    }
    async runStub() {
        if (this.syncStubDetected) return this.info("Stub is already running. No upload is necessary."), this.chip;
        this.info("Uploading stub...");
        let t = $ec3de3bad43fb783$var$Se(this.chip.ROM_TEXT), e = t.split("").map(function(A) {
            return A.charCodeAt(0);
        });
        const i = new Uint8Array(e);
        t = $ec3de3bad43fb783$var$Se(this.chip.ROM_DATA), e = t.split("").map(function(A) {
            return A.charCodeAt(0);
        });
        const s = new Uint8Array(e);
        let a, n = Math.floor((i.length + this.ESP_RAM_BLOCK - 1) / this.ESP_RAM_BLOCK);
        for(await this.memBegin(i.length, n, this.ESP_RAM_BLOCK, this.chip.TEXT_START), a = 0; a < n; a++){
            const A = a * this.ESP_RAM_BLOCK, t = A + this.ESP_RAM_BLOCK;
            await this.memBlock(i.slice(A, t), a);
        }
        for(n = Math.floor((s.length + this.ESP_RAM_BLOCK - 1) / this.ESP_RAM_BLOCK), await this.memBegin(s.length, n, this.ESP_RAM_BLOCK, this.chip.DATA_START), a = 0; a < n; a++){
            const A = a * this.ESP_RAM_BLOCK, t = A + this.ESP_RAM_BLOCK;
            await this.memBlock(s.slice(A, t), a);
        }
        this.info("Running stub..."), await this.memFinish(this.chip.ENTRY);
        for(let A = 0; A < 100; A++){
            const A = await this.transport.read(1e3, 6);
            if (79 === A[0] && 72 === A[1] && 65 === A[2] && 73 === A[3]) return this.info("Stub running..."), this.IS_STUB = !0, this.FLASH_WRITE_SIZE = 16384, this.chip;
        }
        throw new $ec3de3bad43fb783$var$A("Failed to start stub. Unexpected response");
    }
    async changeBaud() {
        this.info("Changing baudrate to " + this.baudrate);
        const A = this.IS_STUB ? this.transport.baudrate : 0, t = this._appendArray(this._intToByteArray(this.baudrate), this._intToByteArray(A)), e = await this.command(this.ESP_CHANGE_BAUDRATE, t);
        this.debug(e[0].toString()), this.info("Changed"), await this.transport.disconnect(), await this._sleep(50), await this.transport.connect(this.baudrate, this.serialOptions);
        try {
            let A = 64;
            for(; A--;){
                try {
                    await this.sync();
                    break;
                } catch (A) {
                    this.debug(A.message);
                }
                await this._sleep(10);
            }
        } catch (A) {
            this.debug(A.message);
        }
    }
    async main(A = "default_reset") {
        await this.detectChip(A);
        const t = await this.chip.getChipDescription(this);
        return this.info("Chip is " + t), this.info("Features: " + await this.chip.getChipFeatures(this)), this.info("Crystal is " + await this.chip.getCrystalFreq(this) + "MHz"), this.info("MAC: " + await this.chip.readMac(this)), await this.chip.readMac(this), void 0 !== this.chip.postConnect && await this.chip.postConnect(this), await this.runStub(), this.romBaudrate !== this.baudrate && await this.changeBaud(), t;
    }
    parseFlashSizeArg(t) {
        if (void 0 === this.chip.FLASH_SIZES[t]) throw new $ec3de3bad43fb783$var$A("Flash size " + t + " is not supported by this chip type. Supported sizes: " + this.chip.FLASH_SIZES);
        return this.chip.FLASH_SIZES[t];
    }
    _updateImageFlashParams(A, t, e, i, s) {
        if (this.debug("_update_image_flash_params " + e + " " + i + " " + s), A.length < 8) return A;
        if (t != this.chip.BOOTLOADER_FLASH_OFFSET) return A;
        if ("keep" === e && "keep" === i && "keep" === s) return this.info("Not changing the image"), A;
        const a = parseInt(A[0]);
        let n = parseInt(A[2]);
        const E = parseInt(A[3]);
        if (a !== this.ESP_IMAGE_MAGIC) return this.info("Warning: Image file at 0x" + t.toString(16) + " doesn't look like an image file, so not changing any flash settings."), A;
        if ("keep" !== i) n = ({
            qio: 0,
            qout: 1,
            dio: 2,
            dout: 3
        })[i];
        let h = 15 & E;
        if ("keep" !== s) h = ({
            "40m": 0,
            "26m": 1,
            "20m": 2,
            "80m": 15
        })[s];
        let r = 240 & E;
        "keep" !== e && (r = this.parseFlashSizeArg(e));
        const g = n << 8 | h + r;
        return this.info("Flash params set to " + g.toString(16)), parseInt(A[2]) !== n << 8 && (A = A.substring(0, 2) + (n << 8).toString() + A.substring(3)), parseInt(A[3]) !== h + r && (A = A.substring(0, 3) + (h + r).toString() + A.substring(4)), A;
    }
    async writeFlash(t) {
        if (this.debug("EspLoader program"), "keep" !== t.flashSize) {
            const e = this.flashSizeBytes(t.flashSize);
            for(let i = 0; i < t.fileArray.length; i++)if (t.fileArray[i].data.length + t.fileArray[i].address > e) throw new $ec3de3bad43fb783$var$A(`File ${i + 1} doesn't fit in the available flash`);
        }
        let e, i;
        !0 === this.IS_STUB && !0 === t.eraseAll && await this.eraseFlash();
        for(let s = 0; s < t.fileArray.length; s++){
            this.debug("Data Length " + t.fileArray[s].data.length), e = t.fileArray[s].data;
            const a = t.fileArray[s].data.length % 4;
            if (a > 0 && (e += "\xff\xff\xff\xff".substring(4 - a)), i = t.fileArray[s].address, this.debug("Image Length " + e.length), 0 === e.length) {
                this.debug("Warning: File is empty");
                continue;
            }
            e = this._updateImageFlashParams(e, i, t.flashSize, t.flashMode, t.flashFreq);
            let n = null;
            t.calculateMD5Hash && (n = t.calculateMD5Hash(e), this.debug("Image MD5 " + n));
            const E = e.length;
            let h;
            if (t.compress) {
                const A = this.bstrToUi8(e);
                e = this.ui8ToBstr($ec3de3bad43fb783$var$ce(A, {
                    level: 9
                })), h = await this.flashDeflBegin(E, e.length, i);
            } else h = await this.flashBegin(E, i);
            let r = 0, g = 0;
            const o = e.length;
            t.reportProgress && t.reportProgress(s, 0, o);
            let B = new Date;
            const w = B.getTime();
            let c = 5e3;
            const C = new $ec3de3bad43fb783$var$Ce({
                chunkSize: 1
            });
            let I = 0;
            for(C.onData = function(A) {
                I += A.byteLength;
            }; e.length > 0;){
                this.debug("Write loop " + i + " " + r + " " + h), this.info("Writing at 0x" + (i + I).toString(16) + "... (" + Math.floor(100 * (r + 1) / h) + "%)");
                const a = this.bstrToUi8(e.slice(0, this.FLASH_WRITE_SIZE));
                if (!t.compress) throw new $ec3de3bad43fb783$var$A("Yet to handle Non Compressed writes");
                {
                    const A = I;
                    C.push(a, !1);
                    const t = I - A;
                    let e = 3e3;
                    this.timeoutPerMb(this.ERASE_WRITE_TIMEOUT_PER_MB, t) > 3e3 && (e = this.timeoutPerMb(this.ERASE_WRITE_TIMEOUT_PER_MB, t)), !1 === this.IS_STUB && (c = e), await this.flashDeflBlock(a, r, c), this.IS_STUB && (c = e);
                }
                g += a.length, e = e.slice(this.FLASH_WRITE_SIZE, e.length), r++, t.reportProgress && t.reportProgress(s, g, o);
            }
            this.IS_STUB && await this.readReg(this.CHIP_DETECT_MAGIC_REG_ADDR, c), B = new Date;
            const _ = B.getTime() - w;
            if (t.compress && this.info("Wrote " + E + " bytes (" + g + " compressed) at 0x" + i.toString(16) + " in " + _ / 1e3 + " seconds."), n) {
                const t = await this.flashMd5sum(i, E);
                if (new String(t).valueOf() != new String(n).valueOf()) throw this.info("File  md5: " + n), this.info("Flash md5: " + t), new $ec3de3bad43fb783$var$A("MD5 of file does not match data in flash!");
                this.info("Hash of data verified.");
            }
        }
        this.info("Leaving..."), this.IS_STUB && (await this.flashBegin(0, 0), t.compress ? await this.flashDeflFinish() : await this.flashFinish());
    }
    async flashId() {
        this.debug("flash_id");
        const A = await this.readFlashId();
        this.info("Manufacturer: " + (255 & A).toString(16));
        const t = A >> 16 & 255;
        this.info("Device: " + (A >> 8 & 255).toString(16) + t.toString(16)), this.info("Detected flash size: " + this.DETECTED_FLASH_SIZES[t]);
    }
    async getFlashSize() {
        this.debug("flash_id");
        const A = await this.readFlashId() >> 16 & 255;
        return this.DETECTED_FLASH_SIZES_NUM[A];
    }
    async hardReset() {
        await this.transport.setRTS(!0), await this._sleep(100), await this.transport.setRTS(!1);
    }
    async softReset() {
        if (this.IS_STUB) {
            if ("ESP8266" != this.chip.CHIP_NAME) throw new $ec3de3bad43fb783$var$A("Soft resetting is currently only supported on ESP8266");
            await this.command(this.ESP_RUN_USER_CODE, void 0, void 0, !1);
        } else await this.flashBegin(0, 0), await this.flashFinish(!1);
    }
}
class $ec3de3bad43fb783$export$c643cc54d6326a6f {
    getEraseSize(A, t) {
        return t;
    }
}
var $ec3de3bad43fb783$var$Fe = 1074521560, $ec3de3bad43fb783$var$Te = "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", $ec3de3bad43fb783$var$ue = 1074520064, $ec3de3bad43fb783$var$pe = "GOv8P9jnC0Bx6AtA8+wLQO3oC0CP6AtA7egLQEnpC0AG6gtAeOoLQCHqC0CB5wtAo+kLQPjpC0Bn6QtAmuoLQI7pC0Ca6gtAXegLQLPoC0Dt6AtASekLQHfoC0BM6wtAs+wLQKXmC0DX7AtApeYLQKXmC0Cl5gtApeYLQKXmC0Cl5gtApeYLQKXmC0Dz6gtApeYLQM3rC0Cz7AtA", $ec3de3bad43fb783$var$ye = 1073605544;
class $ec3de3bad43fb783$var$ke extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
    constructor(){
        super(...arguments), this.CHIP_NAME = "ESP32", this.IMAGE_CHIP_ID = 0, this.EFUSE_RD_REG_BASE = 1073061888, this.DR_REG_SYSCON_BASE = 1073111040, this.UART_CLKDIV_REG = 1072955412, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612856, this.XTAL_CLK_DIVIDER = 1, this.FLASH_SIZES = {
            "1MB": 0,
            "2MB": 16,
            "4MB": 32,
            "8MB": 48,
            "16MB": 64
        }, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 4096, this.SPI_REG_BASE = 1072963584, this.SPI_USR_OFFS = 28, this.SPI_USR1_OFFS = 32, this.SPI_USR2_OFFS = 36, this.SPI_W0_OFFS = 128, this.SPI_MOSI_DLEN_OFFS = 40, this.SPI_MISO_DLEN_OFFS = 44, this.TEXT_START = $ec3de3bad43fb783$var$ue, this.ENTRY = $ec3de3bad43fb783$var$Fe, this.DATA_START = $ec3de3bad43fb783$var$ye, this.ROM_DATA = $ec3de3bad43fb783$var$pe, this.ROM_TEXT = $ec3de3bad43fb783$var$Te;
    }
    async readEfuse(A, t) {
        const e = this.EFUSE_RD_REG_BASE + 4 * t;
        return A.debug("Read efuse " + e), await A.readReg(e);
    }
    async getPkgVersion(A) {
        const t = await this.readEfuse(A, 3);
        let e = t >> 9 & 7;
        return e += (t >> 2 & 1) << 3, e;
    }
    async getChipRevision(A) {
        const t = await this.readEfuse(A, 3), e = await this.readEfuse(A, 5), i = await A.readReg(this.DR_REG_SYSCON_BASE + 124);
        return 0 != (t >> 15 & 1) ? 0 != (e >> 20 & 1) ? 0 != (i >> 31 & 1) ? 3 : 2 : 1 : 0;
    }
    async getChipDescription(A) {
        const t = [
            "ESP32-D0WDQ6",
            "ESP32-D0WD",
            "ESP32-D2WD",
            "",
            "ESP32-U4WDH",
            "ESP32-PICO-D4",
            "ESP32-PICO-V3-02"
        ];
        let e = "";
        const i = await this.getPkgVersion(A), s = await this.getChipRevision(A), a = 3 == s;
        return 0 != (1 & await this.readEfuse(A, 3)) && (t[0] = "ESP32-S0WDQ6", t[1] = "ESP32-S0WD"), a && (t[5] = "ESP32-PICO-V3"), e = i >= 0 && i <= 6 ? t[i] : "Unknown ESP32", !a || 0 !== i && 1 !== i || (e += "-V3"), e + " (revision " + s + ")";
    }
    async getChipFeatures(A) {
        const t = [
            "Wi-Fi"
        ], e = await this.readEfuse(A, 3);
        0 === (2 & e) && t.push(" BT");
        0 !== (1 & e) ? t.push(" Single Core") : t.push(" Dual Core");
        if (0 !== (8192 & e)) 0 !== (4096 & e) ? t.push(" 160MHz") : t.push(" 240MHz");
        const i = await this.getPkgVersion(A);
        -1 !== [
            2,
            4,
            5,
            6
        ].indexOf(i) && t.push(" Embedded Flash"), 6 === i && t.push(" Embedded PSRAM");
        0 !== (await this.readEfuse(A, 4) >> 8 & 31) && t.push(" VRef calibration in efuse");
        0 !== (e >> 14 & 1) && t.push(" BLK3 partially reserved");
        const s = 3 & await this.readEfuse(A, 6);
        return t.push(" Coding Scheme " + [
            "None",
            "3/4",
            "Repeat (UNSUPPORTED)",
            "Invalid"
        ][s]), t;
    }
    async getCrystalFreq(A) {
        const t = await A.readReg(this.UART_CLKDIV_REG) & this.UART_CLKDIV_MASK, e = A.transport.baudrate * t / 1e6 / this.XTAL_CLK_DIVIDER;
        let i;
        return i = e > 33 ? 40 : 26, Math.abs(i - e) > 1 && A.info("WARNING: Unsupported crystal in use"), i;
    }
    _d2h(A) {
        const t = (+A).toString(16);
        return 1 === t.length ? "0" + t : t;
    }
    async readMac(A) {
        let t = await this.readEfuse(A, 1);
        t >>>= 0;
        let e = await this.readEfuse(A, 2);
        e >>>= 0;
        const i = new Uint8Array(6);
        return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
    }
}
var $ec3de3bad43fb783$var$He = Object.freeze({
    __proto__: null,
    ESP32ROM: $ec3de3bad43fb783$var$ke
}), $ec3de3bad43fb783$var$Pe = 1077413532, $ec3de3bad43fb783$var$Oe = "QREixCbCBsa3NwRgEUc3RMg/2Mu3NARgEwQEANxAkYuR57JAIkSSREEBgoCIQBxAE3X1D4KX3bcBEbcHAGBOxoOphwBKyDdJyD8mylLEBs4izLcEAGB9WhMJCQDATBN09D8N4PJAYkQjqDQBQknSRLJJIkoFYYKAiECDJwkAE3X1D4KXfRTjGUT/yb8TBwAMlEGqh2MY5QCFR4XGI6AFAHlVgoAFR2OH5gAJRmONxgB9VYKAQgUTB7ANQYVjlecCiUecwfW3kwbADWMW1QCYwRMFAAyCgJMG0A19VWOV1wCYwRMFsA2CgLd1yT9BEZOFhboGxmE/Y0UFBrd3yT+ThweyA6cHCAPWRwgTdfUPkwYWAMIGwYIjktcIMpcjAKcAA9dHCJFnk4cHBGMe9wI398g/EwcHsqFnupcDpgcItzbJP7d3yT+Thweyk4YGtmMf5gAjpscII6DXCCOSBwghoPlX4wb1/LJAQQGCgCOm1wgjoOcI3bc3JwBgfEudi/X/NzcAYHxLnYv1/4KAQREGxt03tycAYCOmBwI3BwAImMOYQ33/yFeyQBNF9f8FiUEBgoBBEQbG2T993TcHAEC3JwBgmMM3JwBgHEP9/7JAQQGCgEERIsQ3RMg/kwdEAUrAA6kHAQbGJsJjCgkERTc5xb1HEwREAYFEY9YnAQREvYiTtBQAfTeFPxxENwaAABOXxwCZ4DcGAAG39v8AdY+3JgBg2MKQwphCff9BR5HgBUczCelAupcjKCQBHMSyQCJEkkQCSUEBgoABEQbOIswlNzcEzj9sABMFRP+XAMj/54Ag8KqHBUWV57JHk/cHID7GiTc3JwBgHEe3BkAAEwVE/9WPHMeyRZcAyP/ngKDtMzWgAPJAYkQFYYKAQRG3R8g/BsaTh0cBBUcjgOcAE9fFAJjHBWd9F8zDyMf5jTqVqpWxgYzLI6oHAEE3GcETBVAMskBBAYKAAREizDdEyD+TB0QBJsrER07GBs5KyKqJEwREAWPzlQCuhKnAAylEACaZE1nJABxIY1XwABxEY175ArU9fd1IQCaGzoWXAMj/54Ag4RN19Q8BxZMHQAxcyFxAppdcwFxEhY9cxPJAYkTSREJJskkFYYKAaTVtv0ERBsaXAMj/54AA1gNFhQGyQHUVEzUVAEEBgoBBEQbGxTcdyTdHyD8TBwcAXEONxxBHHcK3BgxgmEYNinGbUY+YxgVmuE4TBgbA8Y99dhMG9j9xj9mPvM6yQEEBgoBBEQbGeT8RwQ1FskBBARcDyP9nAIPMQREGxpcAyP/ngEDKQTcBxbJAQQHZv7JAQQGCgEERBsYTBwAMYxrlABMFsA3RPxMFwA2yQEEB6bcTB7AN4xvl/sE3EwXQDfW3QREixCbCBsYqhLMEtQBjF5QAskAiRJJEQQGCgANFBAAFBE0/7bc1cSbLTsf9coVp/XQizUrJUsVWwwbPk4SE+haRk4cJB6aXGAizhOcAKokmhS6ElwDI/+eAgBuThwkHGAgFarqXs4pHQTHkBWd9dZMFhfqTBwcHEwWF+RQIqpczhdcAkwcHB66Xs4XXACrGlwDI/+eAQBgyRcFFlTcBRYViFpH6QGpE2kRKSbpJKkqaSg1hgoCiiWNzigCFaU6G1oVKhZcAyP/ngEDGE3X1DwHtTobWhSaFlwDI/+eAgBNOmTMENEFRtxMFMAZVvxMFAAzZtTFx/XIFZ07XUtVW017PBt8i3SbbStla0WLNZstqyW7H/XcWkRMHBwc+lxwIupc+xiOqB/iqiS6Ksoq2ixE9kwcAAhnBtwcCAD6FlwDI/+eAIAyFZ2PlVxMFZH15EwmJ+pMHBAfKlxgIM4nnAEqFlwDI/+eAoAp9exMMO/mTDIv5EwcEB5MHBAcUCGKX5peBRDMM1wCzjNcAUk1jfE0JY/GkA0GomT+ihQgBjTW5NyKGDAFKhZcAyP/ngIAGopmilGP1RAOzh6RBY/F3AzMEmkBj84oAVoQihgwBToWXAMj/54CAtRN19Q9V3QLMAUR5XY1NowkBAGKFlwDI/+eAwKd9+QNFMQHmhWE0Y08FAOPijf6FZ5OHBweilxgIupfalyOKp/gFBPG34xWl/ZFH4wX09gVnfXWTBwcHkwWF+hMFhfkUCKqXM4XXAJMHBweul7OF1wAqxpcAyP/ngKD8cT0yRcFFZTNRPeUxtwcCABnhkwcAAj6FlwDI/+eAoPmFYhaR+lBqVNpUSlm6WSpamloKW/pLakzaTEpNuk0pYYKAt1dBSRlxk4f3hAFFht6i3KbaytjO1tLU1tLa0N7O4szmyurI7sY+zpcAyP/ngICfQTENzbcEDGCcRDdEyD8TBAQAHMS8TH13Ewf3P1zA+Y+T5wdAvMwTBUAGlwDI/+eAoJUcRPGbk+cXAJzEkTEhwbeHAGA3R9hQk4aHChMHF6qYwhOHBwkjIAcANzcdjyOgBgATB6cSk4YHC5jCk4fHCphDNwYAgFGPmMMjoAYAt0fIPzd3yT+ThwcAEwcHuyGgI6AHAJEH4+3n/kE7kUVoCHE5YTO398g/k4cHsiFnPpcjIPcItwc4QDdJyD+Th4cOIyD5ALd5yT9lPhMJCQCTiQmyYwsFELcnDGBFR7jXhUVFRZcAyP/ngCDjtwU4QAFGk4UFAEVFlwDI/+eAIOQ3NwRgHEs3BQIAk+dHABzLlwDI/+eAIOOXAMj/54Cg87dHAGCcXwnl8YvhFxO1FwCBRZcAyP/ngICWwWe3RMg//RcTBwAQhWZBZrcFAAEBRZOERAENard6yD+XAMj/54AAkSaaE4sKsoOnyQj134OryQiFRyOmCQgjAvECg8cbAAlHIxPhAqMC8QIC1E1HY4HnCFFHY4/nBilHY5/nAIPHOwADxysAogfZjxFHY5bnAIOniwCcQz7UlTmhRUgQQTaDxzsAA8crAKIH2Y8RZ0EHY3T3BBMFsA05PhMFwA0hPhMF4A4JPpkxQbe3BThAAUaThYUDFUWXAMj/54BA1DcHAGBcRxMFAAKT5xcQXMcJt8lHIxPxAk23A8cbANFGY+fmAoVGY+bmAAFMEwTwD4WoeRcTd/cPyUbj6Ob+t3bJPwoHk4ZGuzaXGEMCh5MGBwOT9vYPEUbjadb8Ewf3AhN39w+NRmPr5gi3dsk/CgeThgbANpcYQwKHEwdAAmOY5xAC1B1EAUWFPAFFYTRFNnk+oUVIEH0UZTR19AFMAUQTdfQPhTwTdfwPrTRJNuMeBOqDxxsASUdjY/cuCUfjdvfq9ReT9/cPPUfjYPfqN3fJP4oHEwcHwbqXnEOChwVEnetwEIFFAUWXsMz/54CgAh3h0UVoEKk0AUQxqAVEge+X8Mf/54CAdTM0oAApoCFHY4XnAAVEAUxhtwOsiwADpMsAs2eMANIH9ffv8H+FffHBbCKc/Rx9fTMFjEBV3LN3lQGV48FsMwWMQGPmjAL9fDMFjEBV0DGBl/DH/+eAgHBV+WaU9bcxgZfwx//ngIBvVfFqlNG3QYGX8Mf/54BAblH5MwSUQcG3IUfjiefwAUwTBAAMMbdBR82/QUcFROOc5/aDpcsAA6WLAHU6sb9BRwVE45Ln9gOnCwGRZ2Pl5xyDpUsBA6WLAO/wv4A1v0FHBUTjkuf0g6cLARFnY2X3GgOnywCDpUsBA6WLADOE5wLv8C/+I6wEACMkirAxtwPHBABjDgcQA6eLAMEXEwQADGMT9wDASAFHkwbwDmNG9wKDx1sAA8dLAAFMogfZjwPHawBCB12Pg8d7AOIH2Y/jgfbmEwQQDKm9M4brAANGhgEFB7GO4beDxwQA8cPcRGOYBxLASCOABAB9tWFHY5bnAoOnywEDp4sBg6ZLAQOmCwGDpcsAA6WLAJfwx//ngEBeKowzNKAAKbUBTAVEEbURRwVE45rn5gOliwCBRZfwx//ngABfkbUT9/cA4xoH7JPcRwAThIsAAUx9XeN5nN1IRJfwx//ngIBLGERUQBBA+Y5jB6cBHEITR/f/fY/ZjhTCBQxBBNm/EUdJvUFHBUTjnOfgg6eLAAOnSwEjKPkAIybpAN2zgyXJAMEXkeWJzwFMEwRgDLW7AycJAWNm9wYT9zcA4x4H5AMoCQEBRgFHMwXoQLOG5QBjafcA4wkG1CMoqQAjJtkAmbMzhusAEE4RB5DCBUbpvyFHBUTjlufaAyQJARnAEwSADCMoCQAjJgkAMzSAAEm7AUwTBCAMEbsBTBMEgAwxswFMEwSQDBGzEwcgDWOD5wwTB0AN45DnvAPEOwCDxysAIgRdjJfwx//ngGBJA6zEAEEUY3OEASKM4w4MuMBAYpQxgJxIY1XwAJxEY1v0Cu/wD8513chAYoaThYsBl/DH/+eAYEUBxZMHQAzcyNxA4pfcwNxEs4eHQdzEl/DH/+eAQESJvgllEwUFcQOsywADpIsAl/DH/+eAADa3BwBg2Eu3BgABwRaTV0cBEgd1j72L2Y+zh4cDAUWz1YcCl/DH/+eA4DYTBYA+l/DH/+eAoDIRtoOmSwEDpgsBg6XLAAOliwDv8M/7/bSDxTsAg8crABOFiwGiBd2NwRXv8O/X2bzv8E/HPb+DxzsAA8crABOMiwGiB9mPE40H/wVEt3vJP9xEYwUNAJnDY0yAAGNQBAoTB3AM2MjjnweokweQDGGok4cLu5hDt/fIP5OHB7KZjz7WgyeKsLd8yD9q0JOMTAGTjQu7BUhjc/0ADUhCxjrE7/BPwCJHMkg3Rcg/4oV8EJOGCrIQEBMFxQKX8Mf/54DAMIJXA6eMsIOlDQAzDf1AHY8+nLJXI6TssCqEvpUjoL0Ak4cKsp2NAcWhZ+OS9fZahe/wb8sjoG0Bmb8t9OODB6CTB4AM3Mj1uoOniwDjmwee7/Cv1gllEwUFcZfwx//ngGAg7/Bv0Zfwx//ngKAj0boDpMsA4wcEnO/wL9QTBYA+l/DH/+eAAB7v8A/PApRVuu/wj872UGZU1lRGWbZZJlqWWgZb9ktmTNZMRk22TQlhgoAAAA==", $ec3de3bad43fb783$var$Ue = 1077411840, $ec3de3bad43fb783$var$Ge = "IGvIP3YKOEDGCjhAHgs4QMILOEAuDDhA3As4QEIJOEB+CzhAvgs4QDILOEDyCDhAZgs4QPIIOEBQCjhAlgo4QMYKOEAeCzhAYgo4QKYJOEDWCThAXgo4QIAOOEDGCjhARg04QDgOOEAyCDhAYA44QDIIOEAyCDhAMgg4QDIIOEAyCDhAMgg4QDIIOEAyCDhA4gw4QDIIOEBkDThAOA44QA==", $ec3de3bad43fb783$var$me = 1070164912;
class $ec3de3bad43fb783$var$Ye extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
    constructor(){
        super(...arguments), this.CHIP_NAME = "ESP32-C3", this.IMAGE_CHIP_ID = 5, this.EFUSE_BASE = 1610647552, this.MAC_EFUSE_REG = this.EFUSE_BASE + 68, this.UART_CLKDIV_REG = 1072955412, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612860, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 0, this.FLASH_SIZES = {
            "1MB": 0,
            "2MB": 16,
            "4MB": 32,
            "8MB": 48,
            "16MB": 64
        }, this.SPI_REG_BASE = 1610620928, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.SPI_W0_OFFS = 88, this.TEXT_START = $ec3de3bad43fb783$var$Ue, this.ENTRY = $ec3de3bad43fb783$var$Pe, this.DATA_START = $ec3de3bad43fb783$var$me, this.ROM_DATA = $ec3de3bad43fb783$var$Ge, this.ROM_TEXT = $ec3de3bad43fb783$var$Oe;
    }
    async getPkgVersion(A) {
        const t = this.EFUSE_BASE + 68 + 12;
        return await A.readReg(t) >> 21 & 7;
    }
    async getChipRevision(A) {
        const t = this.EFUSE_BASE + 68 + 12;
        return (await A.readReg(t) & 1835008) >> 18;
    }
    async getChipDescription(A) {
        let t;
        t = 0 === await this.getPkgVersion(A) ? "ESP32-C3" : "unknown ESP32-C3";
        return t += " (revision " + await this.getChipRevision(A) + ")", t;
    }
    async getFlashCap(A) {
        const t = this.EFUSE_BASE + 68 + 12;
        return await A.readReg(t) >> 27 & 7;
    }
    async getFlashVendor(A) {
        const t = this.EFUSE_BASE + 68 + 16;
        return ({
            1: "XMC",
            2: "GD",
            3: "FM",
            4: "TT",
            5: "ZBIT"
        })[await A.readReg(t) >> 0 & 7] || "";
    }
    async getChipFeatures(A) {
        const t = [
            "Wi-Fi",
            "BLE"
        ], e = await this.getFlashCap(A), i = await this.getFlashVendor(A), s = {
            0: null,
            1: "Embedded Flash 4MB",
            2: "Embedded Flash 2MB",
            3: "Embedded Flash 1MB",
            4: "Embedded Flash 8MB"
        }[e], a = void 0 !== s ? s : "Unknown Embedded Flash";
        return null !== s && t.push(`${a} (${i})`), t;
    }
    async getCrystalFreq(A) {
        return 40;
    }
    _d2h(A) {
        const t = (+A).toString(16);
        return 1 === t.length ? "0" + t : t;
    }
    async readMac(A) {
        let t = await A.readReg(this.MAC_EFUSE_REG);
        t >>>= 0;
        let e = await A.readReg(this.MAC_EFUSE_REG + 4);
        e = e >>> 0 & 65535;
        const i = new Uint8Array(6);
        return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
    }
    getEraseSize(A, t) {
        return t;
    }
}
var $ec3de3bad43fb783$var$be = Object.freeze({
    __proto__: null,
    ESP32C3ROM: $ec3de3bad43fb783$var$Ye
}), $ec3de3bad43fb783$var$Ke = 1077413304, $ec3de3bad43fb783$var$xe = "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", $ec3de3bad43fb783$var$Le = 1077411840, $ec3de3bad43fb783$var$Je = "DEDKP+AIOEAsCThAhAk4QFIKOEC+CjhAbAo4QKgHOEAOCjhATgo4QJgJOEBYBzhAzAk4QFgHOEC6CDhA/gg4QCwJOECECThAzAg4QBIIOEBCCDhAyAg4QBYNOEAsCThA1gs4QMoMOECkBjhA9Aw4QKQGOECkBjhApAY4QKQGOECkBjhApAY4QKQGOECkBjhAcgs4QKQGOEDyCzhAygw4QA==", $ec3de3bad43fb783$var$ze = 1070295976;
var $ec3de3bad43fb783$var$Ne = Object.freeze({
    __proto__: null,
    ESP32C2ROM: class extends $ec3de3bad43fb783$var$Ye {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP32-C2", this.IMAGE_CHIP_ID = 12, this.EFUSE_BASE = 1610647552, this.MAC_EFUSE_REG = this.EFUSE_BASE + 64, this.UART_CLKDIV_REG = 1610612756, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612860, this.XTAL_CLK_DIVIDER = 1, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 0, this.FLASH_SIZES = {
                "1MB": 0,
                "2MB": 16,
                "4MB": 32,
                "8MB": 48,
                "16MB": 64
            }, this.SPI_REG_BASE = 1610620928, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.SPI_W0_OFFS = 88, this.TEXT_START = $ec3de3bad43fb783$var$Le, this.ENTRY = $ec3de3bad43fb783$var$Ke, this.DATA_START = $ec3de3bad43fb783$var$ze, this.ROM_DATA = $ec3de3bad43fb783$var$Je, this.ROM_TEXT = $ec3de3bad43fb783$var$xe;
        }
        async getPkgVersion(A) {
            const t = this.EFUSE_BASE + 64 + 4;
            return await A.readReg(t) >> 22 & 7;
        }
        async getChipRevision(A) {
            const t = this.EFUSE_BASE + 64 + 4;
            return (await A.readReg(t) & 3145728) >> 20;
        }
        async getChipDescription(A) {
            let t;
            const e = await this.getPkgVersion(A);
            t = 0 === e || 1 === e ? "ESP32-C2" : "unknown ESP32-C2";
            return t += " (revision " + await this.getChipRevision(A) + ")", t;
        }
        async getChipFeatures(A) {
            return [
                "Wi-Fi",
                "BLE"
            ];
        }
        async getCrystalFreq(A) {
            const t = await A.readReg(this.UART_CLKDIV_REG) & this.UART_CLKDIV_MASK, e = A.transport.baudrate * t / 1e6 / this.XTAL_CLK_DIVIDER;
            let i;
            return i = e > 33 ? 40 : 26, Math.abs(i - e) > 1 && A.info("WARNING: Unsupported crystal in use"), i;
        }
        async changeBaudRate(A) {
            26 === await this.getCrystalFreq(A) && A.changeBaud();
        }
        _d2h(A) {
            const t = (+A).toString(16);
            return 1 === t.length ? "0" + t : t;
        }
        async readMac(A) {
            let t = await A.readReg(this.MAC_EFUSE_REG);
            t >>>= 0;
            let e = await A.readReg(this.MAC_EFUSE_REG + 4);
            e = e >>> 0 & 65535;
            const i = new Uint8Array(6);
            return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
        }
        getEraseSize(A, t) {
            return t;
        }
    }
}), $ec3de3bad43fb783$var$ve = 1082132112, $ec3de3bad43fb783$var$je = "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", $ec3de3bad43fb783$var$Ze = 1082130432, $ec3de3bad43fb783$var$We = "HCuEQEIKgECSCoBA6gqAQI4LgED6C4BAqAuAQA4JgEBKC4BAiguAQP4KgEC+CIBAMguAQL4IgEAcCoBAYgqAQJIKgEDqCoBALgqAQHIJgECiCYBAKgqAQEwOgECSCoBAEg2AQAQOgED+B4BALA6AQP4HgED+B4BA/geAQP4HgED+B4BA/geAQP4HgED+B4BArgyAQP4HgEAwDYBABA6AQA==", $ec3de3bad43fb783$var$Xe = 1082469292;
class $ec3de3bad43fb783$var$qe extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
    constructor(){
        super(...arguments), this.CHIP_NAME = "ESP32-C6", this.IMAGE_CHIP_ID = 13, this.EFUSE_BASE = 1611335680, this.MAC_EFUSE_REG = this.EFUSE_BASE + 68, this.UART_CLKDIV_REG = 1072955412, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612860, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 0, this.FLASH_SIZES = {
            "1MB": 0,
            "2MB": 16,
            "4MB": 32,
            "8MB": 48,
            "16MB": 64
        }, this.SPI_REG_BASE = 1610620928, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.SPI_W0_OFFS = 88, this.TEXT_START = $ec3de3bad43fb783$var$Ze, this.ENTRY = $ec3de3bad43fb783$var$ve, this.DATA_START = $ec3de3bad43fb783$var$Xe, this.ROM_DATA = $ec3de3bad43fb783$var$We, this.ROM_TEXT = $ec3de3bad43fb783$var$je;
    }
    async getPkgVersion(A) {
        const t = this.EFUSE_BASE + 68 + 12;
        return await A.readReg(t) >> 21 & 7;
    }
    async getChipRevision(A) {
        const t = this.EFUSE_BASE + 68 + 12;
        return (await A.readReg(t) & 1835008) >> 18;
    }
    async getChipDescription(A) {
        let t;
        t = 0 === await this.getPkgVersion(A) ? "ESP32-C6" : "unknown ESP32-C6";
        return t += " (revision " + await this.getChipRevision(A) + ")", t;
    }
    async getChipFeatures(A) {
        return [
            "Wi-Fi 6",
            "BT 5",
            "IEEE802.15.4"
        ];
    }
    async getCrystalFreq(A) {
        return 40;
    }
    _d2h(A) {
        const t = (+A).toString(16);
        return 1 === t.length ? "0" + t : t;
    }
    async readMac(A) {
        let t = await A.readReg(this.MAC_EFUSE_REG);
        t >>>= 0;
        let e = await A.readReg(this.MAC_EFUSE_REG + 4);
        e = e >>> 0 & 65535;
        const i = new Uint8Array(6);
        return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
    }
    getEraseSize(A, t) {
        return t;
    }
}
var $ec3de3bad43fb783$var$Ve = Object.freeze({
    __proto__: null,
    ESP32C6ROM: $ec3de3bad43fb783$var$qe
}), $ec3de3bad43fb783$var$$e = 1082132164, $ec3de3bad43fb783$var$Ai = "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", $ec3de3bad43fb783$var$ti = 1082130432, $ec3de3bad43fb783$var$ei = "FACEQG4KgEC+CoBAFguAQOQLgEBQDIBA/guAQDoJgECgC4BA4AuAQCoLgEDqCIBAXguAQOoIgEBICoBAjgqAQL4KgEAWC4BAWgqAQJ4JgEDOCYBAVgqAQKgOgEC+CoBAaA2AQGAOgEAqCIBAiA6AQCoIgEAqCIBAKgiAQCoIgEAqCIBAKgiAQCoIgEAqCIBABA2AQCoIgECGDYBAYA6AQA==", $ec3de3bad43fb783$var$ii = 1082469296;
var $ec3de3bad43fb783$var$si = Object.freeze({
    __proto__: null,
    ESP32C5ROM: class extends $ec3de3bad43fb783$var$qe {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP32-C5", this.IMAGE_CHIP_ID = 23, this.EFUSE_BASE = 1611352064, this.EFUSE_BLOCK1_ADDR = this.EFUSE_BASE + 68, this.MAC_EFUSE_REG = this.EFUSE_BASE + 68, this.UART_CLKDIV_REG = 1610612756, this.TEXT_START = $ec3de3bad43fb783$var$ti, this.ENTRY = $ec3de3bad43fb783$var$$e, this.DATA_START = $ec3de3bad43fb783$var$ii, this.ROM_DATA = $ec3de3bad43fb783$var$ei, this.ROM_TEXT = $ec3de3bad43fb783$var$Ai, this.EFUSE_RD_REG_BASE = this.EFUSE_BASE + 48, this.EFUSE_PURPOSE_KEY0_REG = this.EFUSE_BASE + 52, this.EFUSE_PURPOSE_KEY0_SHIFT = 24, this.EFUSE_PURPOSE_KEY1_REG = this.EFUSE_BASE + 52, this.EFUSE_PURPOSE_KEY1_SHIFT = 28, this.EFUSE_PURPOSE_KEY2_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY2_SHIFT = 0, this.EFUSE_PURPOSE_KEY3_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY3_SHIFT = 4, this.EFUSE_PURPOSE_KEY4_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY4_SHIFT = 8, this.EFUSE_PURPOSE_KEY5_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY5_SHIFT = 12, this.EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG = this.EFUSE_RD_REG_BASE, this.EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT = 1048576, this.EFUSE_SPI_BOOT_CRYPT_CNT_REG = this.EFUSE_BASE + 52, this.EFUSE_SPI_BOOT_CRYPT_CNT_MASK = 1835008, this.EFUSE_SECURE_BOOT_EN_REG = this.EFUSE_BASE + 56, this.EFUSE_SECURE_BOOT_EN_MASK = 1048576, this.IROM_MAP_START = 1107296256, this.IROM_MAP_END = 1115684864, this.DROM_MAP_START = 1115684864, this.DROM_MAP_END = 1124073472, this.PCR_SYSCLK_CONF_REG = 1611227408, this.PCR_SYSCLK_XTAL_FREQ_V = 2130706432, this.PCR_SYSCLK_XTAL_FREQ_S = 24, this.XTAL_CLK_DIVIDER = 1, this.UARTDEV_BUF_NO = 1082520860, this.CHIP_DETECT_MAGIC_VALUE = [
                285294703
            ], this.FLASH_FREQUENCY = {
                "80m": 15,
                "40m": 0,
                "20m": 2
            }, this.MEMORY_MAP = [
                [
                    0,
                    65536,
                    "PADDING"
                ],
                [
                    1115684864,
                    1124073472,
                    "DROM"
                ],
                [
                    1082130432,
                    1082523648,
                    "DRAM"
                ],
                [
                    1082130432,
                    1082523648,
                    "BYTE_ACCESSIBLE"
                ],
                [
                    1073979392,
                    1074003968,
                    "DROM_MASK"
                ],
                [
                    1073741824,
                    1073979392,
                    "IROM_MASK"
                ],
                [
                    1107296256,
                    1115684864,
                    "IROM"
                ],
                [
                    1082130432,
                    1082523648,
                    "IRAM"
                ],
                [
                    1342177280,
                    1342193664,
                    "RTC_IRAM"
                ],
                [
                    1342177280,
                    1342193664,
                    "RTC_DRAM"
                ],
                [
                    1611653120,
                    1611661312,
                    "MEM_INTERNAL2"
                ]
            ], this.UF2_FAMILY_ID = 4145808195, this.EFUSE_MAX_KEY = 5, this.KEY_PURPOSES = {
                0: "USER/EMPTY",
                1: "ECDSA_KEY",
                2: "XTS_AES_256_KEY_1",
                3: "XTS_AES_256_KEY_2",
                4: "XTS_AES_128_KEY",
                5: "HMAC_DOWN_ALL",
                6: "HMAC_DOWN_JTAG",
                7: "HMAC_DOWN_DIGITAL_SIGNATURE",
                8: "HMAC_UP",
                9: "SECURE_BOOT_DIGEST0",
                10: "SECURE_BOOT_DIGEST1",
                11: "SECURE_BOOT_DIGEST2",
                12: "KM_INIT_KEY"
            };
        }
        async getPkgVersion(A) {
            return await A.readReg(this.EFUSE_BLOCK1_ADDR + 8) >> 26 & 7;
        }
        async getMinorChipVersion(A) {
            return await A.readReg(this.EFUSE_BLOCK1_ADDR + 8) >> 0 & 15;
        }
        async getMajorChipVersion(A) {
            return await A.readReg(this.EFUSE_BLOCK1_ADDR + 8) >> 4 & 3;
        }
        async getChipDescription(A) {
            let t;
            t = 0 === await this.getPkgVersion(A) ? "ESP32-C5" : "unknown ESP32-C5";
            return `${t} (revision v${await this.getMajorChipVersion(A)}.${await this.getMinorChipVersion(A)})`;
        }
        async getCrystalFreq(A) {
            const t = await A.readReg(this.UART_CLKDIV_REG) & this.UART_CLKDIV_MASK, e = A.transport.baudrate * t / 1e6 / this.XTAL_CLK_DIVIDER;
            let i;
            return i = e > 45 ? 48 : e > 33 ? 40 : 26, Math.abs(i - e) > 1 && A.info("WARNING: Unsupported crystal in use"), i;
        }
        async getCrystalFreqRomExpect(A) {
            return (await A.readReg(this.PCR_SYSCLK_CONF_REG) & this.PCR_SYSCLK_XTAL_FREQ_V) >> this.PCR_SYSCLK_XTAL_FREQ_S;
        }
    }
}), $ec3de3bad43fb783$var$ai = 1082132112, $ec3de3bad43fb783$var$ni = "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", $ec3de3bad43fb783$var$Ei = 1082130432, $ec3de3bad43fb783$var$hi = "EACDQEIKgECSCoBA6gqAQI4LgED6C4BAqAuAQA4JgEBKC4BAiguAQP4KgEC+CIBAMguAQL4IgEAcCoBAYgqAQJIKgEDqCoBALgqAQHIJgECiCYBAKgqAQFIOgECSCoBAEg2AQAoOgED+B4BAMg6AQP4HgED+B4BA/geAQP4HgED+B4BA/geAQP4HgED+B4BArgyAQP4HgEAwDYBACg6AQA==", $ec3de3bad43fb783$var$ri = 1082403756;
var $ec3de3bad43fb783$var$gi = Object.freeze({
    __proto__: null,
    ESP32H2ROM: class extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP32-H2", this.IMAGE_CHIP_ID = 16, this.EFUSE_BASE = 1610647552, this.MAC_EFUSE_REG = this.EFUSE_BASE + 68, this.UART_CLKDIV_REG = 1072955412, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612860, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 0, this.FLASH_SIZES = {
                "1MB": 0,
                "2MB": 16,
                "4MB": 32,
                "8MB": 48,
                "16MB": 64
            }, this.SPI_REG_BASE = 1610620928, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.SPI_W0_OFFS = 88, this.USB_RAM_BLOCK = 2048, this.UARTDEV_BUF_NO_USB = 3, this.UARTDEV_BUF_NO = 1070526796, this.TEXT_START = $ec3de3bad43fb783$var$Ei, this.ENTRY = $ec3de3bad43fb783$var$ai, this.DATA_START = $ec3de3bad43fb783$var$ri, this.ROM_DATA = $ec3de3bad43fb783$var$hi, this.ROM_TEXT = $ec3de3bad43fb783$var$ni;
        }
        async getChipDescription(A) {
            return this.CHIP_NAME;
        }
        async getChipFeatures(A) {
            return [
                "BLE",
                "IEEE802.15.4"
            ];
        }
        async getCrystalFreq(A) {
            return 32;
        }
        _d2h(A) {
            const t = (+A).toString(16);
            return 1 === t.length ? "0" + t : t;
        }
        async postConnect(A) {
            const t = 255 & await A.readReg(this.UARTDEV_BUF_NO);
            A.debug("In _post_connect " + t), t == this.UARTDEV_BUF_NO_USB && (A.ESP_RAM_BLOCK = this.USB_RAM_BLOCK);
        }
        async readMac(A) {
            let t = await A.readReg(this.MAC_EFUSE_REG);
            t >>>= 0;
            let e = await A.readReg(this.MAC_EFUSE_REG + 4);
            e = e >>> 0 & 65535;
            const i = new Uint8Array(6);
            return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
        }
        getEraseSize(A, t) {
            return t;
        }
    }
}), $ec3de3bad43fb783$var$oi = 1077381696, $ec3de3bad43fb783$var$Bi = "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", $ec3de3bad43fb783$var$wi = 1077379072, $ec3de3bad43fb783$var$ci = "ZCvKP8qNN0CvjjdAcJM3QDqPN0DPjjdAOo83QJmPN0BmkDdA2ZA3QIGQN0BVjTdA/I83QFiQN0C8jzdA+5A3QOaPN0D7kDdAnY43QPqON0A6jzdAmY83QLWON0CWjTdAvJE3QDaTN0ByjDdAVpM3QHKMN0ByjDdAcow3QHKMN0ByjDdAcow3QHKMN0ByjDdAVpE3QHKMN0BRkjdANpM3QAQInwAAAAAAAAAYAQQIBQAAAAAAAAAIAQQIBgAAAAAAAAAAAQQIIQAAAAAAIAAAEQQI3AAAAAAAIAAAEQQIDAAAAAAAIAAAAQQIEgAAAAAAIAAAESAoDAAQAQAA", $ec3de3bad43fb783$var$Ci = 1070279668;
var $ec3de3bad43fb783$var$Ii = Object.freeze({
    __proto__: null,
    ESP32S3ROM: class extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP32-S3", this.IMAGE_CHIP_ID = 9, this.EFUSE_BASE = 1610641408, this.MAC_EFUSE_REG = this.EFUSE_BASE + 68, this.UART_CLKDIV_REG = 1610612756, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612864, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 0, this.FLASH_SIZES = {
                "1MB": 0,
                "2MB": 16,
                "4MB": 32,
                "8MB": 48,
                "16MB": 64
            }, this.SPI_REG_BASE = 1610620928, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.SPI_W0_OFFS = 88, this.USB_RAM_BLOCK = 2048, this.UARTDEV_BUF_NO_USB = 3, this.UARTDEV_BUF_NO = 1070526796, this.TEXT_START = $ec3de3bad43fb783$var$wi, this.ENTRY = $ec3de3bad43fb783$var$oi, this.DATA_START = $ec3de3bad43fb783$var$Ci, this.ROM_DATA = $ec3de3bad43fb783$var$ci, this.ROM_TEXT = $ec3de3bad43fb783$var$Bi;
        }
        async getChipDescription(A) {
            return "ESP32-S3";
        }
        async getFlashCap(A) {
            const t = this.EFUSE_BASE + 68 + 12;
            return await A.readReg(t) >> 27 & 7;
        }
        async getFlashVendor(A) {
            const t = this.EFUSE_BASE + 68 + 16;
            return ({
                1: "XMC",
                2: "GD",
                3: "FM",
                4: "TT",
                5: "BY"
            })[await A.readReg(t) >> 0 & 7] || "";
        }
        async getPsramCap(A) {
            const t = this.EFUSE_BASE + 68 + 16;
            return await A.readReg(t) >> 3 & 3;
        }
        async getPsramVendor(A) {
            const t = this.EFUSE_BASE + 68 + 16;
            return ({
                1: "AP_3v3",
                2: "AP_1v8"
            })[await A.readReg(t) >> 7 & 3] || "";
        }
        async getChipFeatures(A) {
            const t = [
                "Wi-Fi",
                "BLE"
            ], e = await this.getFlashCap(A), i = await this.getFlashVendor(A), s = {
                0: null,
                1: "Embedded Flash 8MB",
                2: "Embedded Flash 4MB"
            }[e], a = void 0 !== s ? s : "Unknown Embedded Flash";
            null !== s && t.push(`${a} (${i})`);
            const n = await this.getPsramCap(A), E = await this.getPsramVendor(A), h = {
                0: null,
                1: "Embedded PSRAM 8MB",
                2: "Embedded PSRAM 2MB"
            }[n], r = void 0 !== h ? h : "Unknown Embedded PSRAM";
            return null !== h && t.push(`${r} (${E})`), t;
        }
        async getCrystalFreq(A) {
            return 40;
        }
        _d2h(A) {
            const t = (+A).toString(16);
            return 1 === t.length ? "0" + t : t;
        }
        async postConnect(A) {
            const t = 255 & await A.readReg(this.UARTDEV_BUF_NO);
            A.debug("In _post_connect " + t), t == this.UARTDEV_BUF_NO_USB && (A.ESP_RAM_BLOCK = this.USB_RAM_BLOCK);
        }
        async readMac(A) {
            let t = await A.readReg(this.MAC_EFUSE_REG);
            t >>>= 0;
            let e = await A.readReg(this.MAC_EFUSE_REG + 4);
            e = e >>> 0 & 65535;
            const i = new Uint8Array(6);
            return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
        }
        getEraseSize(A, t) {
            return t;
        }
    }
}), $ec3de3bad43fb783$var$_i = 1073907696, $ec3de3bad43fb783$var$li = "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", $ec3de3bad43fb783$var$di = 1073905664, $ec3de3bad43fb783$var$Mi = "ZCv9PzaLAkDBiwJAhpACQEqMAkDjiwJASowCQKmMAkByjQJA5Y0CQI2NAkDAigJAC40CQGSNAkDMjAJACI4CQPaMAkAIjgJAr4sCQA6MAkBKjAJAqYwCQMeLAkACiwJAx44CQD2QAkDYiQJAZZACQNiJAkDYiQJA2IkCQNiJAkDYiQJA2IkCQNiJAkDYiQJAZI4CQNiJAkBZjwJAPZACQA==", $ec3de3bad43fb783$var$Di = 1073622012;
var $ec3de3bad43fb783$var$Ri = Object.freeze({
    __proto__: null,
    ESP32S2ROM: class extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP32-S2", this.IMAGE_CHIP_ID = 2, this.MAC_EFUSE_REG = 1061265476, this.EFUSE_BASE = 1061265408, this.UART_CLKDIV_REG = 1061158932, this.UART_CLKDIV_MASK = 1048575, this.UART_DATE_REG_ADDR = 1610612856, this.FLASH_WRITE_SIZE = 1024, this.BOOTLOADER_FLASH_OFFSET = 4096, this.FLASH_SIZES = {
                "1MB": 0,
                "2MB": 16,
                "4MB": 32,
                "8MB": 48,
                "16MB": 64
            }, this.SPI_REG_BASE = 1061167104, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_W0_OFFS = 88, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.TEXT_START = $ec3de3bad43fb783$var$di, this.ENTRY = $ec3de3bad43fb783$var$_i, this.DATA_START = $ec3de3bad43fb783$var$Di, this.ROM_DATA = $ec3de3bad43fb783$var$Mi, this.ROM_TEXT = $ec3de3bad43fb783$var$li;
        }
        async getPkgVersion(A) {
            const t = this.EFUSE_BASE + 68 + 12;
            return await A.readReg(t) >> 21 & 15;
        }
        async getChipDescription(A) {
            const t = [
                "ESP32-S2",
                "ESP32-S2FH16",
                "ESP32-S2FH32"
            ], e = await this.getPkgVersion(A);
            return e >= 0 && e <= 2 ? t[e] : "unknown ESP32-S2";
        }
        async getFlashCap(A) {
            const t = this.EFUSE_BASE + 68 + 12;
            return await A.readReg(t) >> 21 & 15;
        }
        async getPsramCap(A) {
            const t = this.EFUSE_BASE + 68 + 12;
            return await A.readReg(t) >> 28 & 15;
        }
        async getBlock2Version(A) {
            const t = this.EFUSE_BASE + 92 + 16;
            return await A.readReg(t) >> 4 & 7;
        }
        async getChipFeatures(A) {
            const t = [
                "Wi-Fi"
            ], e = {
                0: "No Embedded Flash",
                1: "Embedded Flash 2MB",
                2: "Embedded Flash 4MB"
            }[await this.getFlashCap(A)] || "Unknown Embedded Flash";
            t.push(e);
            const i = {
                0: "No Embedded Flash",
                1: "Embedded PSRAM 2MB",
                2: "Embedded PSRAM 4MB"
            }[await this.getPsramCap(A)] || "Unknown Embedded PSRAM";
            t.push(i);
            const s = {
                0: "No calibration in BLK2 of efuse",
                1: "ADC and temperature sensor calibration in BLK2 of efuse V1",
                2: "ADC and temperature sensor calibration in BLK2 of efuse V2"
            }[await this.getBlock2Version(A)] || "Unknown Calibration in BLK2";
            return t.push(s), t;
        }
        async getCrystalFreq(A) {
            return 40;
        }
        _d2h(A) {
            const t = (+A).toString(16);
            return 1 === t.length ? "0" + t : t;
        }
        async readMac(A) {
            let t = await A.readReg(this.MAC_EFUSE_REG);
            t >>>= 0;
            let e = await A.readReg(this.MAC_EFUSE_REG + 4);
            e = e >>> 0 & 65535;
            const i = new Uint8Array(6);
            return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
        }
        getEraseSize(A, t) {
            return t;
        }
    }
}), $ec3de3bad43fb783$var$Si = 1074843652, $ec3de3bad43fb783$var$Qi = "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", $ec3de3bad43fb783$var$fi = 1074843648, $ec3de3bad43fb783$var$Fi = "CIH+PwUFBAACAwcAAwMLALnXEEDv1xBAHdgQQLrYEEBo5xBAHtkQQHTZEEDA2RBAaOcQQILaEED/2hBAwNsQQGjnEEBo5xBAWNwQQGjnEEA33xBAAOAQQDvgEEBo5xBAaOcQQNfgEEBo5xBAv+EQQGXiEECj4xBAY+QQQDTlEEBo5xBAaOcQQGjnEEBo5xBAYuYQQGjnEEBX5xBAkN0QQI/YEECm5RBAq9oQQPzZEEBo5xBA7OYQQDHnEEBo5xBAaOcQQGjnEEBo5xBAaOcQQGjnEEBo5xBAaOcQQCLaEEBf2hBAvuUQQAEAAAACAAAAAwAAAAQAAAAFAAAABwAAAAkAAAANAAAAEQAAABkAAAAhAAAAMQAAAEEAAABhAAAAgQAAAMEAAAABAQAAgQEAAAECAAABAwAAAQQAAAEGAAABCAAAAQwAAAEQAAABGAAAASAAAAEwAAABQAAAAWAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAABAAAAAgAAAAIAAAADAAAAAwAAAAQAAAAEAAAABQAAAAUAAAAGAAAABgAAAAcAAAAHAAAACAAAAAgAAAAJAAAACQAAAAoAAAAKAAAACwAAAAsAAAAMAAAADAAAAA0AAAANAAAAAAAAAAAAAAADAAAABAAAAAUAAAAGAAAABwAAAAgAAAAJAAAACgAAAAsAAAANAAAADwAAABEAAAATAAAAFwAAABsAAAAfAAAAIwAAACsAAAAzAAAAOwAAAEMAAABTAAAAYwAAAHMAAACDAAAAowAAAMMAAADjAAAAAgEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAQAAAAEAAAABAAAAAgAAAAIAAAACAAAAAgAAAAMAAAADAAAAAwAAAAMAAAAEAAAABAAAAAQAAAAEAAAABQAAAAUAAAAFAAAABQAAAAAAAAAAAAAAAAAAABAREgAIBwkGCgULBAwDDQIOAQ8AAQEAAAEAAAAEAAAA", $ec3de3bad43fb783$var$Ti = 1073720488;
var $ec3de3bad43fb783$var$ui = Object.freeze({
    __proto__: null,
    ESP8266ROM: class extends $ec3de3bad43fb783$export$c643cc54d6326a6f {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP8266", this.CHIP_DETECT_MAGIC_VALUE = [
                4293968129
            ], this.EFUSE_RD_REG_BASE = 1072693328, this.UART_CLKDIV_REG = 1610612756, this.UART_CLKDIV_MASK = 1048575, this.XTAL_CLK_DIVIDER = 2, this.FLASH_WRITE_SIZE = 16384, this.BOOTLOADER_FLASH_OFFSET = 0, this.UART_DATE_REG_ADDR = 0, this.FLASH_SIZES = {
                "512KB": 0,
                "256KB": 16,
                "1MB": 32,
                "2MB": 48,
                "4MB": 64,
                "2MB-c1": 80,
                "4MB-c1": 96,
                "8MB": 128,
                "16MB": 144
            }, this.SPI_REG_BASE = 1610613248, this.SPI_USR_OFFS = 28, this.SPI_USR1_OFFS = 32, this.SPI_USR2_OFFS = 36, this.SPI_MOSI_DLEN_OFFS = 0, this.SPI_MISO_DLEN_OFFS = 0, this.SPI_W0_OFFS = 64, this.TEXT_START = $ec3de3bad43fb783$var$fi, this.ENTRY = $ec3de3bad43fb783$var$Si, this.DATA_START = $ec3de3bad43fb783$var$Ti, this.ROM_DATA = $ec3de3bad43fb783$var$Fi, this.ROM_TEXT = $ec3de3bad43fb783$var$Qi, this.getChipFeatures = async (A)=>{
                const t = [
                    "WiFi"
                ];
                return "ESP8285" == await this.getChipDescription(A) && t.push("Embedded Flash"), t;
            };
        }
        async readEfuse(A, t) {
            const e = this.EFUSE_RD_REG_BASE + 4 * t;
            return A.debug("Read efuse " + e), await A.readReg(e);
        }
        async getChipDescription(A) {
            const t = await this.readEfuse(A, 2);
            return 0 != (16 & await this.readEfuse(A, 0) | 65536 & t) ? "ESP8285" : "ESP8266EX";
        }
        async getCrystalFreq(A) {
            const t = await A.readReg(this.UART_CLKDIV_REG) & this.UART_CLKDIV_MASK, e = A.transport.baudrate * t / 1e6 / this.XTAL_CLK_DIVIDER;
            let i;
            return i = e > 33 ? 40 : 26, Math.abs(i - e) > 1 && A.info("WARNING: Detected crystal freq " + e + "MHz is quite different to normalized freq " + i + "MHz. Unsupported crystal in use?"), i;
        }
        _d2h(A) {
            const t = (+A).toString(16);
            return 1 === t.length ? "0" + t : t;
        }
        async readMac(A) {
            let t = await this.readEfuse(A, 0);
            t >>>= 0;
            let e = await this.readEfuse(A, 1);
            e >>>= 0;
            let i = await this.readEfuse(A, 3);
            i >>>= 0;
            const s = new Uint8Array(6);
            return 0 != i ? (s[0] = i >> 16 & 255, s[1] = i >> 8 & 255, s[2] = 255 & i) : 0 == (e >> 16 & 255) ? (s[0] = 24, s[1] = 254, s[2] = 52) : 1 == (e >> 16 & 255) ? (s[0] = 172, s[1] = 208, s[2] = 116) : A.error("Unknown OUI"), s[3] = e >> 8 & 255, s[4] = 255 & e, s[5] = t >> 24 & 255, this._d2h(s[0]) + ":" + this._d2h(s[1]) + ":" + this._d2h(s[2]) + ":" + this._d2h(s[3]) + ":" + this._d2h(s[4]) + ":" + this._d2h(s[5]);
        }
        getEraseSize(A, t) {
            return t;
        }
    }
}), $ec3de3bad43fb783$var$pi = 1341195918, $ec3de3bad43fb783$var$yi = "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", $ec3de3bad43fb783$var$ki = 1341194240, $ec3de3bad43fb783$var$Hi = "EAD1TwYK8U9WCvFPrgrxT4QL8U/wC/FPngvxT9QI8U9AC/FPgAvxT8IK8U+ECPFP9grxT4QI8U/gCfFPJgrxT1YK8U+uCvFP8gnxTzgJ8U9oCfFP7gnxT0AO8U9WCvFPCA3xTwAO8U/EB/FPJA7xT8QH8U/EB/FPxAfxT8QH8U/EB/FPxAfxT8QH8U/EB/FPpAzxT8QH8U8mDfFPAA7xTw==", $ec3de3bad43fb783$var$Pi = 1341533100;
var $ec3de3bad43fb783$var$Oi = Object.freeze({
    __proto__: null,
    ESP32P4ROM: class extends $ec3de3bad43fb783$var$ke {
        constructor(){
            super(...arguments), this.CHIP_NAME = "ESP32-P4", this.IMAGE_CHIP_ID = 18, this.IROM_MAP_START = 1073741824, this.IROM_MAP_END = 1275068416, this.DROM_MAP_START = 1073741824, this.DROM_MAP_END = 1275068416, this.BOOTLOADER_FLASH_OFFSET = 8192, this.CHIP_DETECT_MAGIC_VALUE = [
                0,
                182303440
            ], this.UART_DATE_REG_ADDR = 1343004812, this.EFUSE_BASE = 1343410176, this.EFUSE_BLOCK1_ADDR = this.EFUSE_BASE + 68, this.MAC_EFUSE_REG = this.EFUSE_BASE + 68, this.SPI_REG_BASE = 1342754816, this.SPI_USR_OFFS = 24, this.SPI_USR1_OFFS = 28, this.SPI_USR2_OFFS = 32, this.SPI_MOSI_DLEN_OFFS = 36, this.SPI_MISO_DLEN_OFFS = 40, this.SPI_W0_OFFS = 88, this.EFUSE_RD_REG_BASE = this.EFUSE_BASE + 48, this.EFUSE_PURPOSE_KEY0_REG = this.EFUSE_BASE + 52, this.EFUSE_PURPOSE_KEY0_SHIFT = 24, this.EFUSE_PURPOSE_KEY1_REG = this.EFUSE_BASE + 52, this.EFUSE_PURPOSE_KEY1_SHIFT = 28, this.EFUSE_PURPOSE_KEY2_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY2_SHIFT = 0, this.EFUSE_PURPOSE_KEY3_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY3_SHIFT = 4, this.EFUSE_PURPOSE_KEY4_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY4_SHIFT = 8, this.EFUSE_PURPOSE_KEY5_REG = this.EFUSE_BASE + 56, this.EFUSE_PURPOSE_KEY5_SHIFT = 12, this.EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT_REG = this.EFUSE_RD_REG_BASE, this.EFUSE_DIS_DOWNLOAD_MANUAL_ENCRYPT = 1048576, this.EFUSE_SPI_BOOT_CRYPT_CNT_REG = this.EFUSE_BASE + 52, this.EFUSE_SPI_BOOT_CRYPT_CNT_MASK = 1835008, this.EFUSE_SECURE_BOOT_EN_REG = this.EFUSE_BASE + 56, this.EFUSE_SECURE_BOOT_EN_MASK = 1048576, this.PURPOSE_VAL_XTS_AES256_KEY_1 = 2, this.PURPOSE_VAL_XTS_AES256_KEY_2 = 3, this.PURPOSE_VAL_XTS_AES128_KEY = 4, this.SUPPORTS_ENCRYPTED_FLASH = !0, this.FLASH_ENCRYPTED_WRITE_ALIGN = 16, this.MEMORY_MAP = [
                [
                    0,
                    65536,
                    "PADDING"
                ],
                [
                    1073741824,
                    1275068416,
                    "DROM"
                ],
                [
                    1341128704,
                    1341784064,
                    "DRAM"
                ],
                [
                    1341128704,
                    1341784064,
                    "BYTE_ACCESSIBLE"
                ],
                [
                    1337982976,
                    1338114048,
                    "DROM_MASK"
                ],
                [
                    1337982976,
                    1338114048,
                    "IROM_MASK"
                ],
                [
                    1073741824,
                    1275068416,
                    "IROM"
                ],
                [
                    1341128704,
                    1341784064,
                    "IRAM"
                ],
                [
                    1343258624,
                    1343291392,
                    "RTC_IRAM"
                ],
                [
                    1343258624,
                    1343291392,
                    "RTC_DRAM"
                ],
                [
                    1611653120,
                    1611661312,
                    "MEM_INTERNAL2"
                ]
            ], this.UF2_FAMILY_ID = 1026592404, this.EFUSE_MAX_KEY = 5, this.KEY_PURPOSES = {
                0: "USER/EMPTY",
                1: "ECDSA_KEY",
                2: "XTS_AES_256_KEY_1",
                3: "XTS_AES_256_KEY_2",
                4: "XTS_AES_128_KEY",
                5: "HMAC_DOWN_ALL",
                6: "HMAC_DOWN_JTAG",
                7: "HMAC_DOWN_DIGITAL_SIGNATURE",
                8: "HMAC_UP",
                9: "SECURE_BOOT_DIGEST0",
                10: "SECURE_BOOT_DIGEST1",
                11: "SECURE_BOOT_DIGEST2",
                12: "KM_INIT_KEY"
            }, this.TEXT_START = $ec3de3bad43fb783$var$ki, this.ENTRY = $ec3de3bad43fb783$var$pi, this.DATA_START = $ec3de3bad43fb783$var$Pi, this.ROM_DATA = $ec3de3bad43fb783$var$Hi, this.ROM_TEXT = $ec3de3bad43fb783$var$yi;
        }
        async getPkgVersion(A) {
            const t = this.EFUSE_BLOCK1_ADDR + 8;
            return await A.readReg(t) >> 27 & 7;
        }
        async getMinorChipVersion(A) {
            const t = this.EFUSE_BLOCK1_ADDR + 8;
            return await A.readReg(t) >> 0 & 15;
        }
        async getMajorChipVersion(A) {
            const t = this.EFUSE_BLOCK1_ADDR + 8;
            return await A.readReg(t) >> 4 & 3;
        }
        async getChipDescription(A) {
            return `${0 === await this.getPkgVersion(A) ? "ESP32-P4" : "unknown ESP32-P4"} (revision v${await this.getMajorChipVersion(A)}.${await this.getMinorChipVersion(A)})`;
        }
        async getChipFeatures(A) {
            return [
                "High-Performance MCU"
            ];
        }
        async getCrystalFreq(A) {
            return 40;
        }
        async getFlashVoltage(A) {}
        async overrideVddsdio(A) {
            A.debug("VDD_SDIO overrides are not supported for ESP32-P4");
        }
        async readMac(A) {
            let t = await A.readReg(this.MAC_EFUSE_REG);
            t >>>= 0;
            let e = await A.readReg(this.MAC_EFUSE_REG + 4);
            e = e >>> 0 & 65535;
            const i = new Uint8Array(6);
            return i[0] = e >> 8 & 255, i[1] = 255 & e, i[2] = t >> 24 & 255, i[3] = t >> 16 & 255, i[4] = t >> 8 & 255, i[5] = 255 & t, this._d2h(i[0]) + ":" + this._d2h(i[1]) + ":" + this._d2h(i[2]) + ":" + this._d2h(i[3]) + ":" + this._d2h(i[4]) + ":" + this._d2h(i[5]);
        }
        async getFlashCryptConfig(A) {}
        async getSecureBootEnabled(A) {
            return await A.readReg(this.EFUSE_SECURE_BOOT_EN_REG) & this.EFUSE_SECURE_BOOT_EN_MASK;
        }
        async getKeyBlockPurpose(A, t) {
            if (t < 0 || t > this.EFUSE_MAX_KEY) return void A.debug(`Valid key block numbers must be in range 0-${this.EFUSE_MAX_KEY}`);
            const e = [
                [
                    this.EFUSE_PURPOSE_KEY0_REG,
                    this.EFUSE_PURPOSE_KEY0_SHIFT
                ],
                [
                    this.EFUSE_PURPOSE_KEY1_REG,
                    this.EFUSE_PURPOSE_KEY1_SHIFT
                ],
                [
                    this.EFUSE_PURPOSE_KEY2_REG,
                    this.EFUSE_PURPOSE_KEY2_SHIFT
                ],
                [
                    this.EFUSE_PURPOSE_KEY3_REG,
                    this.EFUSE_PURPOSE_KEY3_SHIFT
                ],
                [
                    this.EFUSE_PURPOSE_KEY4_REG,
                    this.EFUSE_PURPOSE_KEY4_SHIFT
                ],
                [
                    this.EFUSE_PURPOSE_KEY5_REG,
                    this.EFUSE_PURPOSE_KEY5_SHIFT
                ]
            ], [i, s] = e[t];
            return await A.readReg(i) >> s & 15;
        }
        async isFlashEncryptionKeyValid(A) {
            const t = [];
            for(let e = 0; e <= this.EFUSE_MAX_KEY; e++){
                const i = await this.getKeyBlockPurpose(A, e);
                t.push(i);
            }
            t.find((A)=>A === this.PURPOSE_VAL_XTS_AES128_KEY);
            return !0;
            const e = t.find((A)=>A === this.PURPOSE_VAL_XTS_AES256_KEY_1), i = t.find((A)=>A === this.PURPOSE_VAL_XTS_AES256_KEY_2);
            return true;
        }
    }
});


/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of
 * the License at
 *
 *    https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in
 * writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing
 * permissions and limitations under the License.
 */ "use strict";
var $d2bbb828b377f05f$export$24d5ae9391ffe6e0;
(function(SerialPolyfillProtocol) {
    SerialPolyfillProtocol[SerialPolyfillProtocol["UsbCdcAcm"] = 0] = "UsbCdcAcm";
})($d2bbb828b377f05f$export$24d5ae9391ffe6e0 || ($d2bbb828b377f05f$export$24d5ae9391ffe6e0 = {}));
const $d2bbb828b377f05f$var$kSetLineCoding = 0x20;
const $d2bbb828b377f05f$var$kSetControlLineState = 0x22;
const $d2bbb828b377f05f$var$kSendBreak = 0x23;
const $d2bbb828b377f05f$var$kDefaultBufferSize = 255;
const $d2bbb828b377f05f$var$kDefaultDataBits = 8;
const $d2bbb828b377f05f$var$kDefaultParity = "none";
const $d2bbb828b377f05f$var$kDefaultStopBits = 1;
const $d2bbb828b377f05f$var$kAcceptableDataBits = [
    16,
    8,
    7,
    6,
    5
];
const $d2bbb828b377f05f$var$kAcceptableStopBits = [
    1,
    2
];
const $d2bbb828b377f05f$var$kAcceptableParity = [
    "none",
    "even",
    "odd"
];
const $d2bbb828b377f05f$var$kParityIndexMapping = [
    "none",
    "odd",
    "even"
];
const $d2bbb828b377f05f$var$kStopBitsIndexMapping = [
    1,
    1.5,
    2
];
const $d2bbb828b377f05f$var$kDefaultPolyfillOptions = {
    protocol: $d2bbb828b377f05f$export$24d5ae9391ffe6e0.UsbCdcAcm,
    usbControlInterfaceClass: 2,
    usbTransferInterfaceClass: 10
};
/**
 * Utility function to get the interface implementing a desired class.
 * @param {USBDevice} device The USB device.
 * @param {number} classCode The desired interface class.
 * @return {USBInterface} The first interface found that implements the desired
 * class.
 * @throws TypeError if no interface is found.
 */ function $d2bbb828b377f05f$var$findInterface(device, classCode) {
    const configuration = device.configurations[0];
    for (const iface of configuration.interfaces){
        const alternate = iface.alternates[0];
        if (alternate.interfaceClass === classCode) return iface;
    }
    throw new TypeError(`Unable to find interface with class ${classCode}.`);
}
/**
 * Utility function to get an endpoint with a particular direction.
 * @param {USBInterface} iface The interface to search.
 * @param {USBDirection} direction The desired transfer direction.
 * @return {USBEndpoint} The first endpoint with the desired transfer direction.
 * @throws TypeError if no endpoint is found.
 */ function $d2bbb828b377f05f$var$findEndpoint(iface, direction) {
    const alternate = iface.alternates[0];
    for (const endpoint of alternate.endpoints){
        if (endpoint.direction == direction) return endpoint;
    }
    throw new TypeError(`Interface ${iface.interfaceNumber} does not have an ` + `${direction} endpoint.`);
}
/**
 * Implementation of the underlying source API[1] which reads data from a USB
 * endpoint. This can be used to construct a ReadableStream.
 *
 * [1]: https://streams.spec.whatwg.org/#underlying-source-api
 */ class $d2bbb828b377f05f$var$UsbEndpointUnderlyingSource {
    /**
     * Constructs a new UnderlyingSource that will pull data from the specified
     * endpoint on the given USB device.
     *
     * @param {USBDevice} device
     * @param {USBEndpoint} endpoint
     * @param {function} onError function to be called on error
     */ constructor(device, endpoint, onError){
        this.type = "bytes";
        this.device_ = device;
        this.endpoint_ = endpoint;
        this.onError_ = onError;
    }
    /**
     * Reads a chunk of data from the device.
     *
     * @param {ReadableByteStreamController} controller
     */ pull(controller) {
        (async ()=>{
            var _a;
            let chunkSize;
            if (controller.desiredSize) {
                const d = controller.desiredSize / this.endpoint_.packetSize;
                chunkSize = Math.ceil(d) * this.endpoint_.packetSize;
            } else chunkSize = this.endpoint_.packetSize;
            try {
                const result = await this.device_.transferIn(this.endpoint_.endpointNumber, chunkSize);
                if (result.status != "ok") {
                    controller.error(`USB error: ${result.status}`);
                    this.onError_();
                }
                if ((_a = result.data) === null || _a === void 0 ? void 0 : _a.buffer) {
                    const chunk = new Uint8Array(result.data.buffer, result.data.byteOffset, result.data.byteLength);
                    controller.enqueue(chunk);
                }
            } catch (error) {
                controller.error(error.toString());
                this.onError_();
            }
        })();
    }
}
/**
 * Implementation of the underlying sink API[2] which writes data to a USB
 * endpoint. This can be used to construct a WritableStream.
 *
 * [2]: https://streams.spec.whatwg.org/#underlying-sink-api
 */ class $d2bbb828b377f05f$var$UsbEndpointUnderlyingSink {
    /**
     * Constructs a new UnderlyingSink that will write data to the specified
     * endpoint on the given USB device.
     *
     * @param {USBDevice} device
     * @param {USBEndpoint} endpoint
     * @param {function} onError function to be called on error
     */ constructor(device, endpoint, onError){
        this.device_ = device;
        this.endpoint_ = endpoint;
        this.onError_ = onError;
    }
    /**
     * Writes a chunk to the device.
     *
     * @param {Uint8Array} chunk
     * @param {WritableStreamDefaultController} controller
     */ async write(chunk, controller) {
        try {
            const result = await this.device_.transferOut(this.endpoint_.endpointNumber, chunk);
            if (result.status != "ok") {
                controller.error(result.status);
                this.onError_();
            }
        } catch (error) {
            controller.error(error.toString());
            this.onError_();
        }
    }
}
class $d2bbb828b377f05f$export$237d90817cb05a2f {
    /**
     * constructor taking a WebUSB device that creates a SerialPort instance.
     * @param {USBDevice} device A device acquired from the WebUSB API
     * @param {SerialPolyfillOptions} polyfillOptions Optional options to
     * configure the polyfill.
     */ constructor(device, polyfillOptions){
        this.polyfillOptions_ = Object.assign(Object.assign({}, $d2bbb828b377f05f$var$kDefaultPolyfillOptions), polyfillOptions);
        this.outputSignals_ = {
            dataTerminalReady: false,
            requestToSend: false,
            break: false
        };
        this.device_ = device;
        this.controlInterface_ = $d2bbb828b377f05f$var$findInterface(this.device_, this.polyfillOptions_.usbControlInterfaceClass);
        this.transferInterface_ = $d2bbb828b377f05f$var$findInterface(this.device_, this.polyfillOptions_.usbTransferInterfaceClass);
        this.inEndpoint_ = $d2bbb828b377f05f$var$findEndpoint(this.transferInterface_, "in");
        this.outEndpoint_ = $d2bbb828b377f05f$var$findEndpoint(this.transferInterface_, "out");
    }
    /**
     * Getter for the readable attribute. Constructs a new ReadableStream as
     * necessary.
     * @return {ReadableStream} the current readable stream
     */ get readable() {
        var _a;
        if (!this.readable_ && this.device_.opened) this.readable_ = new ReadableStream(new $d2bbb828b377f05f$var$UsbEndpointUnderlyingSource(this.device_, this.inEndpoint_, ()=>{
            this.readable_ = null;
        }), {
            highWaterMark: (_a = this.serialOptions_.bufferSize) !== null && _a !== void 0 ? _a : $d2bbb828b377f05f$var$kDefaultBufferSize
        });
        return this.readable_;
    }
    /**
     * Getter for the writable attribute. Constructs a new WritableStream as
     * necessary.
     * @return {WritableStream} the current writable stream
     */ get writable() {
        var _a;
        if (!this.writable_ && this.device_.opened) this.writable_ = new WritableStream(new $d2bbb828b377f05f$var$UsbEndpointUnderlyingSink(this.device_, this.outEndpoint_, ()=>{
            this.writable_ = null;
        }), new ByteLengthQueuingStrategy({
            highWaterMark: (_a = this.serialOptions_.bufferSize) !== null && _a !== void 0 ? _a : $d2bbb828b377f05f$var$kDefaultBufferSize
        }));
        return this.writable_;
    }
    /**
     * a function that opens the device and claims all interfaces needed to
     * control and communicate to and from the serial device
     * @param {SerialOptions} options Object containing serial options
     * @return {Promise<void>} A promise that will resolve when device is ready
     * for communication
     */ async open(options) {
        this.serialOptions_ = options;
        this.validateOptions();
        try {
            await this.device_.open();
            if (this.device_.configuration === null) await this.device_.selectConfiguration(1);
            await this.device_.claimInterface(this.controlInterface_.interfaceNumber);
            if (this.controlInterface_ !== this.transferInterface_) await this.device_.claimInterface(this.transferInterface_.interfaceNumber);
            await this.setLineCoding();
            await this.setSignals({
                dataTerminalReady: true
            });
        } catch (error) {
            if (this.device_.opened) await this.device_.close();
            throw new Error("Error setting up device: " + error.toString());
        }
    }
    /**
     * Closes the port.
     *
     * @return {Promise<void>} A promise that will resolve when the port is
     * closed.
     */ async close() {
        const promises = [];
        if (this.readable_) promises.push(this.readable_.cancel());
        if (this.writable_) promises.push(this.writable_.abort());
        await Promise.all(promises);
        this.readable_ = null;
        this.writable_ = null;
        if (this.device_.opened) {
            await this.setSignals({
                dataTerminalReady: false,
                requestToSend: false
            });
            await this.device_.close();
        }
    }
    /**
     * Forgets the port.
     *
     * @return {Promise<void>} A promise that will resolve when the port is
     * forgotten.
     */ async forget() {
        return this.device_.forget();
    }
    /**
     * A function that returns properties of the device.
     * @return {SerialPortInfo} Device properties.
     */ getInfo() {
        return {
            usbVendorId: this.device_.vendorId,
            usbProductId: this.device_.productId
        };
    }
    /**
     * A function used to change the serial settings of the device
     * @param {object} options the object which carries serial settings data
     * @return {Promise<void>} A promise that will resolve when the options are
     * set
     */ reconfigure(options) {
        this.serialOptions_ = Object.assign(Object.assign({}, this.serialOptions_), options);
        this.validateOptions();
        return this.setLineCoding();
    }
    /**
     * Sets control signal state for the port.
     * @param {SerialOutputSignals} signals The signals to enable or disable.
     * @return {Promise<void>} a promise that is resolved when the signal state
     * has been changed.
     */ async setSignals(signals) {
        this.outputSignals_ = Object.assign(Object.assign({}, this.outputSignals_), signals);
        if (signals.dataTerminalReady !== undefined || signals.requestToSend !== undefined) {
            // The Set_Control_Line_State command expects a bitmap containing the
            // values of all output signals that should be enabled or disabled.
            //
            // Ref: USB CDC specification version 1.1 §6.2.14.
            const value = (this.outputSignals_.dataTerminalReady ? 1 : 0) | (this.outputSignals_.requestToSend ? 2 : 0);
            await this.device_.controlTransferOut({
                "requestType": "class",
                "recipient": "interface",
                "request": $d2bbb828b377f05f$var$kSetControlLineState,
                "value": value,
                "index": this.controlInterface_.interfaceNumber
            });
        }
        if (signals.break !== undefined) {
            // The SendBreak command expects to be given a duration for how long the
            // break signal should be asserted. Passing 0xFFFF enables the signal
            // until 0x0000 is send.
            //
            // Ref: USB CDC specification version 1.1 §6.2.15.
            const value = this.outputSignals_.break ? 0xFFFF : 0x0000;
            await this.device_.controlTransferOut({
                "requestType": "class",
                "recipient": "interface",
                "request": $d2bbb828b377f05f$var$kSendBreak,
                "value": value,
                "index": this.controlInterface_.interfaceNumber
            });
        }
    }
    /**
     * Checks the serial options for validity and throws an error if it is
     * not valid
     */ validateOptions() {
        if (!this.isValidBaudRate(this.serialOptions_.baudRate)) throw new RangeError("invalid Baud Rate " + this.serialOptions_.baudRate);
        if (!this.isValidDataBits(this.serialOptions_.dataBits)) throw new RangeError("invalid dataBits " + this.serialOptions_.dataBits);
        if (!this.isValidStopBits(this.serialOptions_.stopBits)) throw new RangeError("invalid stopBits " + this.serialOptions_.stopBits);
        if (!this.isValidParity(this.serialOptions_.parity)) throw new RangeError("invalid parity " + this.serialOptions_.parity);
    }
    /**
     * Checks the baud rate for validity
     * @param {number} baudRate the baud rate to check
     * @return {boolean} A boolean that reflects whether the baud rate is valid
     */ isValidBaudRate(baudRate) {
        return baudRate % 1 === 0;
    }
    /**
     * Checks the data bits for validity
     * @param {number} dataBits the data bits to check
     * @return {boolean} A boolean that reflects whether the data bits setting is
     * valid
     */ isValidDataBits(dataBits) {
        if (typeof dataBits === "undefined") return true;
        return $d2bbb828b377f05f$var$kAcceptableDataBits.includes(dataBits);
    }
    /**
     * Checks the stop bits for validity
     * @param {number} stopBits the stop bits to check
     * @return {boolean} A boolean that reflects whether the stop bits setting is
     * valid
     */ isValidStopBits(stopBits) {
        if (typeof stopBits === "undefined") return true;
        return $d2bbb828b377f05f$var$kAcceptableStopBits.includes(stopBits);
    }
    /**
     * Checks the parity for validity
     * @param {string} parity the parity to check
     * @return {boolean} A boolean that reflects whether the parity is valid
     */ isValidParity(parity) {
        if (typeof parity === "undefined") return true;
        return $d2bbb828b377f05f$var$kAcceptableParity.includes(parity);
    }
    /**
     * sends the options alog the control interface to set them on the device
     * @return {Promise} a promise that will resolve when the options are set
     */ async setLineCoding() {
        var _a, _b, _c;
        // Ref: USB CDC specification version 1.1 §6.2.12.
        const buffer = new ArrayBuffer(7);
        const view = new DataView(buffer);
        view.setUint32(0, this.serialOptions_.baudRate, true);
        view.setUint8(4, $d2bbb828b377f05f$var$kStopBitsIndexMapping.indexOf((_a = this.serialOptions_.stopBits) !== null && _a !== void 0 ? _a : $d2bbb828b377f05f$var$kDefaultStopBits));
        view.setUint8(5, $d2bbb828b377f05f$var$kParityIndexMapping.indexOf((_b = this.serialOptions_.parity) !== null && _b !== void 0 ? _b : $d2bbb828b377f05f$var$kDefaultParity));
        view.setUint8(6, (_c = this.serialOptions_.dataBits) !== null && _c !== void 0 ? _c : $d2bbb828b377f05f$var$kDefaultDataBits);
        const result = await this.device_.controlTransferOut({
            "requestType": "class",
            "recipient": "interface",
            "request": $d2bbb828b377f05f$var$kSetLineCoding,
            "value": 0x00,
            "index": this.controlInterface_.interfaceNumber
        }, buffer);
        if (result.status != "ok") throw new DOMException("NetworkError", "Failed to set line coding.");
    }
}
/** implementation of the global navigator.serial object */ class $d2bbb828b377f05f$var$Serial {
    /**
     * Requests permission to access a new port.
     *
     * @param {SerialPortRequestOptions} options
     * @param {SerialPolyfillOptions} polyfillOptions
     * @return {Promise<SerialPort>}
     */ async requestPort(options, polyfillOptions) {
        polyfillOptions = Object.assign(Object.assign({}, $d2bbb828b377f05f$var$kDefaultPolyfillOptions), polyfillOptions);
        const usbFilters = [];
        if (options && options.filters) for (const filter of options.filters){
            const usbFilter = {
                classCode: polyfillOptions.usbControlInterfaceClass
            };
            if (filter.usbVendorId !== undefined) usbFilter.vendorId = filter.usbVendorId;
            if (filter.usbProductId !== undefined) usbFilter.productId = filter.usbProductId;
            usbFilters.push(usbFilter);
        }
        if (usbFilters.length === 0) usbFilters.push({
            classCode: polyfillOptions.usbControlInterfaceClass
        });
        const device = await navigator.usb.requestDevice({
            "filters": usbFilters
        });
        const port = new $d2bbb828b377f05f$export$237d90817cb05a2f(device, polyfillOptions);
        return port;
    }
    /**
     * Get the set of currently available ports.
     *
     * @param {SerialPolyfillOptions} polyfillOptions Polyfill configuration that
     * should be applied to these ports.
     * @return {Promise<SerialPort[]>} a promise that is resolved with a list of
     * ports.
     */ async getPorts(polyfillOptions) {
        polyfillOptions = Object.assign(Object.assign({}, $d2bbb828b377f05f$var$kDefaultPolyfillOptions), polyfillOptions);
        const devices = await navigator.usb.getDevices();
        const ports = [];
        devices.forEach((device)=>{
            try {
                const port = new $d2bbb828b377f05f$export$237d90817cb05a2f(device, polyfillOptions);
                ports.push(port);
            } catch (e) {
            // Skip unrecognized port.
            }
        });
        return ports;
    }
}
const $d2bbb828b377f05f$export$6c2c9a00e27c07e8 = new $d2bbb828b377f05f$var$Serial();


const $382e02c9bbd5d50b$var$baudrates = document.getElementById("baudrates");
const $382e02c9bbd5d50b$var$connectButton = document.getElementById("connectButton");
const $382e02c9bbd5d50b$var$traceButton = document.getElementById("copyTraceButton");
const $382e02c9bbd5d50b$var$disconnectButton = document.getElementById("disconnectButton");
const $382e02c9bbd5d50b$var$resetButton = document.getElementById("resetButton");
const $382e02c9bbd5d50b$var$eraseButton = document.getElementById("eraseButton");
const $382e02c9bbd5d50b$var$programButton = document.getElementById("programButton");
const $382e02c9bbd5d50b$var$filesDiv = document.getElementById("files");
const $382e02c9bbd5d50b$var$terminal = document.getElementById("terminal");
const $382e02c9bbd5d50b$var$programDiv = document.getElementById("program");
const $382e02c9bbd5d50b$var$lblBaudrate = document.getElementById("lblBaudrate");
const $382e02c9bbd5d50b$var$lblConnTo = document.getElementById("lblConnTo");
const $382e02c9bbd5d50b$var$table = document.getElementById("fileTable");
if (!navigator.serial && navigator.usb) navigator.serial = (0, $d2bbb828b377f05f$export$6c2c9a00e27c07e8);
const $382e02c9bbd5d50b$var$term = new Terminal({
    cols: 120,
    rows: 40
});
$382e02c9bbd5d50b$var$term.open($382e02c9bbd5d50b$var$terminal);
let $382e02c9bbd5d50b$var$device = null;
let $382e02c9bbd5d50b$var$transport;
let $382e02c9bbd5d50b$var$chip = null;
let $382e02c9bbd5d50b$var$esploader;
const $382e02c9bbd5d50b$var$fileArray = [];
$382e02c9bbd5d50b$var$disconnectButton.style.display = "none";
$382e02c9bbd5d50b$var$traceButton.style.display = "none";
$382e02c9bbd5d50b$var$eraseButton.style.display = "none";
$382e02c9bbd5d50b$var$resetButton.style.display = "none";
$382e02c9bbd5d50b$var$filesDiv.style.display = "none";
const $382e02c9bbd5d50b$var$espLoaderTerminal = {
    clean () {
        $382e02c9bbd5d50b$var$term.clear();
    },
    writeLine (data) {
        $382e02c9bbd5d50b$var$term.writeln(data);
    },
    write (data) {
        $382e02c9bbd5d50b$var$term.write(data);
    }
};
function $382e02c9bbd5d50b$var$cleanUp() {
    $382e02c9bbd5d50b$var$device = null;
    $382e02c9bbd5d50b$var$transport = null;
    $382e02c9bbd5d50b$var$chip = null;
}
$382e02c9bbd5d50b$var$traceButton.onclick = async ()=>{
    if ($382e02c9bbd5d50b$var$transport) $382e02c9bbd5d50b$var$transport.returnTrace();
};
$382e02c9bbd5d50b$var$resetButton.onclick = async ()=>{
    if ($382e02c9bbd5d50b$var$transport) {
        await $382e02c9bbd5d50b$var$transport.setDTR(false);
        await new Promise((resolve)=>setTimeout(resolve, 100));
        await $382e02c9bbd5d50b$var$transport.setDTR(true);
    }
};
$382e02c9bbd5d50b$var$eraseButton.onclick = async ()=>{
    $382e02c9bbd5d50b$var$eraseButton.disabled = true;
    try {
        await $382e02c9bbd5d50b$var$esploader.eraseFlash();
    } catch (e) {
        console.error(e);
        $382e02c9bbd5d50b$var$term.writeln(`Error: ${e.message}`);
    } finally{
        $382e02c9bbd5d50b$var$eraseButton.disabled = false;
    }
};
$382e02c9bbd5d50b$var$connectButton.onclick = async ()=>{
    if ($382e02c9bbd5d50b$var$device === null) {
        $382e02c9bbd5d50b$var$device = await navigator.serial.requestPort({});
        $382e02c9bbd5d50b$var$transport = new (0, $ec3de3bad43fb783$export$86495b081fef8e52)($382e02c9bbd5d50b$var$device, true);
    }
    try {
        const flashOptions = {
            transport: $382e02c9bbd5d50b$var$transport,
            baudrate: parseInt($382e02c9bbd5d50b$var$baudrates.value),
            terminal: $382e02c9bbd5d50b$var$espLoaderTerminal
        };
        $382e02c9bbd5d50b$var$esploader = new (0, $ec3de3bad43fb783$export$b0f7a6c745790308)(flashOptions);
        $382e02c9bbd5d50b$var$lblBaudrate.style.display = "none";
        $382e02c9bbd5d50b$var$baudrates.style.display = "none";
        $382e02c9bbd5d50b$var$connectButton.style.display = "none";
        $382e02c9bbd5d50b$var$chip = await $382e02c9bbd5d50b$var$esploader.main();
        if (!$382e02c9bbd5d50b$var$chip.startsWith("ESP32-C6")) {
            $382e02c9bbd5d50b$var$term.writeln("Error! Wrong chip connected. Expected is ESP32-C6");
            await $382e02c9bbd5d50b$var$transport.disconnect();
            $382e02c9bbd5d50b$var$cleanUp();
            $382e02c9bbd5d50b$var$lblBaudrate.style.display = "initial";
            $382e02c9bbd5d50b$var$baudrates.style.display = "initial";
            $382e02c9bbd5d50b$var$connectButton.style.display = "initial";
            return;
        }
    } catch (e) {
        console.error(e);
        $382e02c9bbd5d50b$var$term.writeln(`Error: ${e.message}`);
    }
    console.log("Settings done for :" + $382e02c9bbd5d50b$var$chip);
    $382e02c9bbd5d50b$var$lblBaudrate.style.display = "none";
    $382e02c9bbd5d50b$var$baudrates.style.display = "none";
    $382e02c9bbd5d50b$var$lblConnTo.innerHTML = "Connected to device: " + $382e02c9bbd5d50b$var$chip;
    $382e02c9bbd5d50b$var$lblConnTo.style.display = "block";
    $382e02c9bbd5d50b$var$connectButton.style.display = "none";
    $382e02c9bbd5d50b$var$disconnectButton.style.display = "initial";
    $382e02c9bbd5d50b$var$traceButton.style.display = "initial";
    $382e02c9bbd5d50b$var$eraseButton.style.display = "initial";
    $382e02c9bbd5d50b$var$filesDiv.style.display = "initial";
};
$382e02c9bbd5d50b$var$disconnectButton.onclick = async ()=>{
    if ($382e02c9bbd5d50b$var$transport) await $382e02c9bbd5d50b$var$transport.disconnect();
    $382e02c9bbd5d50b$var$term.reset();
    $382e02c9bbd5d50b$var$lblBaudrate.style.display = "initial";
    $382e02c9bbd5d50b$var$baudrates.style.display = "initial";
    $382e02c9bbd5d50b$var$connectButton.style.display = "initial";
    $382e02c9bbd5d50b$var$disconnectButton.style.display = "none";
    $382e02c9bbd5d50b$var$traceButton.style.display = "none";
    $382e02c9bbd5d50b$var$eraseButton.style.display = "none";
    $382e02c9bbd5d50b$var$lblConnTo.style.display = "none";
    $382e02c9bbd5d50b$var$filesDiv.style.display = "none";
    $382e02c9bbd5d50b$var$programButton.style.display = "initial";
    $382e02c9bbd5d50b$var$resetButton.style.display = "none";
    for(let index = 1; index < $382e02c9bbd5d50b$var$table.rows.length; index++)$382e02c9bbd5d50b$var$table.rows[index].cells[3].childNodes[0].value = "0";
    $382e02c9bbd5d50b$var$cleanUp();
};
$382e02c9bbd5d50b$var$programButton.onclick = async ()=>{
    const progressBars = [];
    for(let index = 1; index < $382e02c9bbd5d50b$var$table.rows.length; index++){
        const row = $382e02c9bbd5d50b$var$table.rows[index];
        const progressBar = row.cells[3].childNodes[0];
        progressBar.textContent = "0";
        progressBars.push(progressBar);
    }
    try {
        const flashOptions = {
            fileArray: $382e02c9bbd5d50b$var$fileArray,
            eraseAll: false,
            compress: true,
            flashSize: "keep",
            flashFreq: "80MHz",
            flashMode: "DIO",
            reportProgress: (fileIndex, written, total)=>{
                progressBars[fileIndex].value = written / total * 100;
            },
            calculateMD5Hash: (image)=>CryptoJS.MD5(CryptoJS.enc.Latin1.parse(image))
        };
        $382e02c9bbd5d50b$var$programButton.style.display = "none";
        $382e02c9bbd5d50b$var$eraseButton.style.display = "none";
        await $382e02c9bbd5d50b$var$esploader.writeFlash(flashOptions);
    } catch (e) {
        console.error(e);
        $382e02c9bbd5d50b$var$programButton.style.display = "initial";
        $382e02c9bbd5d50b$var$term.writeln(`Error: ${e.message}`);
    } finally{
        // Switch to console
        await $382e02c9bbd5d50b$var$transport.disconnect();
        await $382e02c9bbd5d50b$var$transport.connect(115200);
        await $382e02c9bbd5d50b$var$transport.setDTR(false);
        await new Promise((resolve)=>setTimeout(resolve, 100));
        await $382e02c9bbd5d50b$var$transport.setDTR(true);
        $382e02c9bbd5d50b$var$resetButton.style.display = "initial";
        while(true){
            const val = await $382e02c9bbd5d50b$var$transport.rawRead();
            if (typeof val !== "undefined") $382e02c9bbd5d50b$var$term.write(val);
            else {
                console.log("Leaving monitor");
                break;
            }
        }
    }
};
// addFileButton.onclick = () => {
function $382e02c9bbd5d50b$var$addFileRow(addr, path, size) {
    const rowCount = $382e02c9bbd5d50b$var$table.rows.length;
    const row = $382e02c9bbd5d50b$var$table.insertRow(rowCount);
    //Column 1 - Offset
    const cell1 = row.insertCell(0);
    const element1 = document.createElement("span");
    element1.innerHTML = addr;
    cell1.appendChild(element1);
    // Column 2 - File selector
    const cell2 = row.insertCell(1);
    const element2 = document.createElement("span");
    element2.innerHTML = path;
    cell2.appendChild(element2);
    // Column 3 - File Size
    const cell3 = row.insertCell(2);
    const element3 = document.createElement("span");
    element3.innerHTML = size + " B";
    cell3.appendChild(element3);
    // Column 4  - Progress
    const cell4 = row.insertCell(3);
    cell4.classList.add("progress-cell");
    cell4.innerHTML = `<progress value="0" max="100" style="width:100%"'></progress>`;
}
function $382e02c9bbd5d50b$var$addFileForFlashing(outArray, addr, path) {
    console.log("load", addr, path);
    var request = new XMLHttpRequest();
    request.open("GET", path, true);
    request.responseType = "blob";
    request.onload = function() {
        console.log("blob", addr, path, request.response);
        var size = request.response.size;
        var reader = new FileReader();
        reader.onload = function(e) {
            outArray.push({
                data: e.target.result,
                address: parseInt(addr)
            });
            console.log("push", addr, path);
            $382e02c9bbd5d50b$var$addFileRow(addr, path, size);
        };
        reader.readAsBinaryString(request.response);
    };
    request.send();
}
$382e02c9bbd5d50b$var$addFileForFlashing($382e02c9bbd5d50b$var$fileArray, "0x0", "bootloader.bin");
$382e02c9bbd5d50b$var$addFileForFlashing($382e02c9bbd5d50b$var$fileArray, "0x8000", "partition-table.bin");
$382e02c9bbd5d50b$var$addFileForFlashing($382e02c9bbd5d50b$var$fileArray, "0xd000", "ota_data_initial.bin");
$382e02c9bbd5d50b$var$addFileForFlashing($382e02c9bbd5d50b$var$fileArray, "0x10000", "network_adapter.bin");


//# sourceMappingURL=index.4e6113ae.js.map
