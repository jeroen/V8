#ifndef V8_NATIVE_JS_H
#define V8_NATIVE_JS_H

/* Converts a tree of nodes created from an R object into JavaScript values.
 * Nodes are arrays [tag, ...]:
 *   [0]                              NULL
 *   [1, Int32Array, null, scalar]    logical
 *   [2, Int32Array, null, scalar]    integer
 *   [3, Float64Array, mask, scalar]  double, mask is a Uint8Array marking NA or null
 *   [4, Int32Array, levels, scalar]  character or factor, 1-based codes into levels
 *   [5, Uint8Array]                  raw
 *   [6, children, names]             list, names is null for unnamed lists
 *   [7, columns, names, nrow]        data frame
 *   [8, vector, dims]                matrix or array
 * NA is -2147483648 in integer, logical and code vectors. */
static const char* native_decode_js = R"JS(
(function (root) {
  "use strict";
  const NA = -2147483648;

  function element(node, i) {
    switch (node[0]) {
      case 1: { const v = node[1][i]; return v === NA ? null : v !== 0; }
      case 2: { const v = node[1][i]; return v === NA ? null : v; }
      case 3: { const m = node[2]; return (m !== null && m[i] === 1) ? null : node[1][i]; }
      case 4: { const v = node[1][i]; return v === NA ? null : node[2][v - 1]; }
    }
    return null;
  }

  function define(obj, key, value) {
    if (key === "__proto__") {
      Object.defineProperty(obj, key, {value: value, enumerable: true, writable: true, configurable: true});
    } else {
      obj[key] = value;
    }
  }

  function array(node, dims) {
    function build(level, base, mult) {
      if (level === dims.length) return element(node, base);
      const d = dims[level];
      const out = new Array(d);
      for (let i = 0; i < d; i++) out[i] = build(level + 1, base + i * mult, mult * d);
      return out;
    }
    return build(0, 0, 1);
  }

  function rows(names, columns, n) {
    const p = names.length;
    const kinds = new Array(p);
    const data = new Array(p);
    const extra = new Array(p);
    const has_na = new Array(p);
    for (let j = 0; j < p; j++) {
      const col = columns[j];
      const tag = col[0];
      if (tag >= 1 && tag <= 4) {
        kinds[j] = tag;
        data[j] = col[1];
        extra[j] = col[2];
        if (tag === 3) {
          has_na[j] = col[2] !== null;
        } else {
          has_na[j] = col[1].indexOf(NA) !== -1;
        }
      } else {
        kinds[j] = 6;
        extra[j] = null;
        has_na[j] = false;
        if (tag === 6) {
          const vals = new Array(n);
          for (let i = 0; i < n; i++) vals[i] = decode(col[1][i]);
          data[j] = vals;
        } else if (tag === 0) {
          data[j] = new Array(n).fill(null);
        } else {
          data[j] = decode(col);
        }
      }
    }
    const values = new Array(p);
    const checks = new Array(p);
    let any_na = false;
    for (let j = 0; j < p; j++) {
      const c = "c" + j + "[i]";
      switch (kinds[j]) {
        case 1: values[j] = "(" + c + " !== 0)"; checks[j] = c + " !== NA"; break;
        case 2: values[j] = c; checks[j] = c + " !== NA"; break;
        case 3: values[j] = c; checks[j] = "x" + j + "[i] === 0"; break;
        case 4: values[j] = "x" + j + "[" + c + " - 1]"; checks[j] = c + " !== NA"; break;
        case 6: values[j] = c; checks[j] = "true"; break;
      }
      if (has_na[j]) any_na = true;
    }
    let src = "\"use strict\";\nconst NA = -2147483648;\n";
    for (let j = 0; j < p; j++)
      src += "const c" + j + " = data[" + j + "], x" + j + " = extra[" + j + "];\n";
    src += "const rows = new Array(n);\nfor (let i = 0; i < n; i++) {\n";
    if (any_na) {
      src += "const row = {};\n";
      for (let j = 0; j < p; j++) {
        const key = JSON.stringify(names[j]);
        const store = (names[j] === "__proto__") ?
          "Object.defineProperty(row, " + key + ", {value: " + values[j] + ", enumerable: true, writable: true, configurable: true});" :
          "row[" + key + "] = " + values[j] + ";";
        src += has_na[j] ? "if (" + checks[j] + ") " + store + "\n" : store + "\n";
      }
      src += "rows[i] = row;\n";
    } else {
      const fields = new Array(p);
      for (let j = 0; j < p; j++)
        fields[j] = "[" + JSON.stringify(names[j]) + "]: " + values[j];
      src += "rows[i] = {" + fields.join(", ") + "};\n";
    }
    src += "}\nreturn rows;\n";
    return (new Function("data", "extra", "n", src))(data, extra, n);
  }

  function decode(node) {
    switch (node[0]) {
      case 0:
        return {};
      case 1:
      case 2:
      case 3:
      case 4: {
        if (node[3]) return element(node, 0);
        const n = node[1].length;
        const out = new Array(n);
        for (let i = 0; i < n; i++) out[i] = element(node, i);
        return out;
      }
      case 5:
        return node[1];
      case 6: {
        const children = node[1];
        const names = node[2];
        const n = children.length;
        if (names === null) {
          const out = new Array(n);
          for (let i = 0; i < n; i++) out[i] = decode(children[i]);
          return out;
        }
        const out = {};
        for (let i = 0; i < n; i++) define(out, names[i] === "" ? String(i + 1) : names[i], decode(children[i]));
        return out;
      }
      case 7:
        return rows(node[2], node[1], node[3]);
      case 8:
        return array(node[1], node[2]);
    }
    throw new TypeError("Unknown node type " + node[0]);
  }

  return decode(root);
})
)JS";

/* Converts a JavaScript value into the same kind of nodes, but with
 * codes in character vectors starting at 1 and 0 meaning NA, and masks
 * at position 2 for logical, integer and double vectors. */
static const char* native_encode_js = R"JS(
(function (root) {
  "use strict";
  let depth = 0;

  function resolve(v) {
    if (v !== null && typeof v === "object" && typeof v.toJSON === "function") return v.toJSON();
    return v;
  }

  function missing(v) {
    return v === undefined || v === null || typeof v === "function" || typeof v === "symbol";
  }

  function enter() {
    if (++depth > 1000) throw new RangeError("Maximum nesting depth exceeded (circular structure?)");
  }

  function special_number(s) {
    return s === "NA" || s === "NaN" || s === "Inf" || s === "-Inf";
  }

  function vector(vals) {
    const n = vals.length;
    let has_str = false, has_num = false, has_bool = false, all_int = true, only_special = true, only_na = true;
    for (let i = 0; i < n; i++) {
      const v = vals[i];
      switch (typeof v) {
        case "number":
          has_num = true;
          if (all_int && !(Number.isInteger(v) && v > -2147483648 && v <= 2147483647)) all_int = false;
          break;
        case "bigint":
          has_num = true;
          all_int = false;
          break;
        case "boolean":
          has_bool = true;
          break;
        case "string":
          has_str = true;
          if (only_special && !special_number(v)) only_special = false;
          if (only_na && v !== "NA") only_na = false;
          break;
      }
    }
    if (has_str && !(has_num && only_special)) {
      const codes = new Int32Array(n);
      const levels = [];
      const index = new Map();
      for (let i = 0; i < n; i++) {
        let v = vals[i];
        switch (typeof v) {
          case "string": break;
          case "number": case "bigint": v = String(v); break;
          case "boolean": v = v ? "TRUE" : "FALSE"; break;
          default: continue;
        }
        let code = index.get(v);
        if (code === undefined) {
          levels.push(v);
          code = levels.length;
          index.set(v, code);
        }
        codes[i] = code;
      }
      return [4, codes, levels];
    }
    if (has_num) {
      let mask = null;
      if (all_int && (!has_str || only_na)) {
        const out = new Int32Array(n);
        for (let i = 0; i < n; i++) {
          const v = vals[i];
          if (typeof v === "number") {
            out[i] = v;
          } else if (typeof v === "boolean") {
            out[i] = v ? 1 : 0;
          } else {
            if (mask === null) mask = new Uint8Array(n);
            mask[i] = 1;
          }
        }
        return [2, out, mask];
      }
      const out = new Float64Array(n);
      for (let i = 0; i < n; i++) {
        const v = vals[i];
        switch (typeof v) {
          case "number": out[i] = v; break;
          case "bigint": out[i] = Number(v); break;
          case "boolean": out[i] = v ? 1 : 0; break;
          case "string":
            if (v === "NaN") { out[i] = NaN; break; }
            if (v === "Inf") { out[i] = Infinity; break; }
            if (v === "-Inf") { out[i] = -Infinity; break; }
          default:
            if (mask === null) mask = new Uint8Array(n);
            mask[i] = 1;
        }
      }
      return [3, out, mask];
    }
    const out = new Int32Array(n);
    let mask = null;
    for (let i = 0; i < n; i++) {
      const v = vals[i];
      if (typeof v === "boolean") {
        out[i] = v ? 1 : 0;
      } else {
        if (mask === null) mask = new Uint8Array(n);
        mask[i] = 1;
      }
    }
    return [1, out, mask];
  }

  function classify(vals) {
    const n = vals.length;
    let objects = 0, arrays = 0, others = 0, nulls = 0, primitives = 0;
    for (let i = 0; i < n; i++) {
      const v = resolve(vals[i]);
      vals[i] = v;
      if (missing(v)) {
        nulls++;
      } else if (typeof v === "object") {
        if (Array.isArray(v)) arrays++;
        else if (v instanceof ArrayBuffer || ArrayBuffer.isView(v)) others++;
        else objects++;
      } else {
        primitives++;
      }
    }
    return {objects: objects, arrays: arrays, others: others, nulls: nulls, primitives: primitives};
  }

  function column(rows, key) {
    const n = rows.length;
    if (key === "__proto__") {
      const vals = new Array(n);
      for (let i = 0; i < n; i++) {
        const r = rows[i];
        vals[i] = (r !== null && Object.prototype.hasOwnProperty.call(r, key)) ? r[key] : undefined;
      }
      return vals;
    }
    const src = "\"use strict\";\nconst n = rows.length;\nconst vals = new Array(n);\n" +
      "for (let i = 0; i < n; i++) {\nconst r = rows[i];\nvals[i] = (r === null) ? undefined : r[" +
      JSON.stringify(key) + "];\n}\nreturn vals;";
    return (new Function("rows", src))(rows);
  }

  function frame(vals) {
    const n = vals.length;
    const rows = new Array(n);
    const keys = [];
    const seen = new Set();
    for (let i = 0; i < n; i++) {
      const r = vals[i];
      if (missing(r)) {
        rows[i] = null;
        continue;
      }
      rows[i] = r;
      const rk = Object.keys(r);
      for (let j = 0; j < rk.length; j++) {
        if (!seen.has(rk[j])) {
          seen.add(rk[j]);
          keys.push(rk[j]);
        }
      }
    }
    const p = keys.length;
    const columns = new Array(p);
    enter();
    for (let j = 0; j < p; j++) {
      const col = column(rows, keys[j]);
      const c = classify(col);
      if (c.objects + c.arrays + c.others === 0) {
        columns[j] = vector(col);
      } else if (c.objects > 0 && c.arrays + c.others + c.primitives === 0) {
        columns[j] = frame(col);
      } else {
        const children = new Array(n);
        for (let i = 0; i < n; i++) children[i] = encode(col[i]);
        columns[j] = [6, children, null];
      }
    }
    depth--;
    return [7, columns, keys, n];
  }

  function dims_of(vals) {
    const dims = [vals.length];
    let level = vals;
    while (level.length > 0 && Array.isArray(level[0])) {
      dims.push(level[0].length);
      level = level[0];
    }
    if (dims.length < 2) return null;
    function check(v, d) {
      if (d === dims.length) return !(v !== null && typeof v === "object") && typeof v !== "function";
      if (!Array.isArray(v) || v.length !== dims[d]) return false;
      for (let i = 0; i < v.length; i++) if (!check(v[i], d + 1)) return false;
      return true;
    }
    for (let i = 0; i < vals.length; i++) if (!check(vals[i], 1)) return null;
    return dims;
  }

  function matrix(vals, dims) {
    let total = 1;
    for (let d = 0; d < dims.length; d++) total *= dims[d];
    const flat = new Array(total);
    function fill(v, level, base, mult) {
      if (level === dims.length) {
        flat[base] = v;
        return;
      }
      for (let i = 0; i < v.length; i++) fill(v[i], level + 1, base + i * mult, mult * dims[level]);
    }
    fill(vals, 0, 0, 1);
    return [8, vector(flat), dims];
  }

  function list(vals) {
    const n = vals.length;
    const children = new Array(n);
    for (let i = 0; i < n; i++) children[i] = encode(vals[i]);
    return [6, children, null];
  }

  function encode_array(arr) {
    const n = arr.length;
    if (n === 0) return [6, [], null];
    enter();
    const vals = arr.slice();
    const c = classify(vals);
    let out;
    if (c.objects + c.arrays + c.others === 0) {
      out = vector(vals);
    } else if (c.objects > 0 && c.arrays + c.others + c.primitives === 0) {
      out = frame(vals);
    } else if (c.arrays === n) {
      const dims = dims_of(vals);
      out = (dims !== null && dims[dims.length - 1] > 0) ? matrix(vals, dims) : list(vals);
    } else {
      out = list(vals);
    }
    depth--;
    return out;
  }

  function encode(value) {
    const v = resolve(value);
    if (missing(v)) return [0];
    switch (typeof v) {
      case "boolean": return [1, Int32Array.of(v ? 1 : 0), null];
      case "number": return vector([v]);
      case "bigint": return [3, Float64Array.of(Number(v)), null];
      case "string": return [4, Int32Array.of(1), [v]];
    }
    if (v instanceof ArrayBuffer) return [5, new Uint8Array(v)];
    if (ArrayBuffer.isView(v)) return [5, v];
    if (Array.isArray(v)) return encode_array(v);
    enter();
    const keys = Object.keys(v);
    const children = [];
    const names = [];
    for (let j = 0; j < keys.length; j++) {
      const x = resolve(v[keys[j]]);
      if (x === undefined || typeof x === "function" || typeof x === "symbol") continue;
      names.push(keys[j]);
      children.push(encode(x));
    }
    depth--;
    return [6, children, names];
  }

  return encode(root);
})
)JS";

#endif
