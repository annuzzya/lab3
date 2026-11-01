(() => {
  "use strict";

  const CATEGORY = {
    TranslationUnit: "decl", FunctionDecl: "decl", ParmVarDecl: "decl", VarDecl: "decl", DeclStmt: "decl",
    IntegerLiteral: "lit", FloatingLiteral: "lit", StringLiteral: "lit",
    DeclRefExpr: "ref", CallExpr: "ref", ArraySubscriptExpr: "ref",
    BinaryOperator: "op", LogicalOperator: "op", AssignOperator: "op", UnaryOperator: "op",
    IncDecOperator: "op", ConditionalOperator: "op", CStyleCastExpr: "op"
  };
  const COLORS = { decl: "#3563e9", stmt: "#8a4fd8", op: "#e07a1f", ref: "#d6336c", lit: "#2f9e44" };
  const C_KEYWORDS = ["int", "double", "void", "if", "else", "while", "do", "for", "return", "break", "continue"];
  const JS_KEYWORDS = ["function", "let", "const", "if", "else", "while", "do", "for", "return", "break",
    "continue", "new", "throw", "typeof", "switch", "case", "default", "undefined", "null", "true", "false"];
  const DURATION = 300;

  const $ = id => document.getElementById(id);
  const svg = d3.select("#tree");
  const viewport = svg.append("g");
  const linkLayer = viewport.append("g");
  const nodeLayer = viewport.append("g");
  const zoom = d3.zoom().scaleExtent([0.15, 2.5]).on("zoom", e => viewport.attr("transform", e.transform));
  svg.call(zoom).on("dblclick.zoom", null);

  const layout = d3.tree().nodeSize([26, 300]);
  let root = null;
  let selected = null;
  let showTypes = true;
  let uid = 0;

  const category = d => CATEGORY[d.data.kind] || "stmt";
  const color = d => COLORS[category(d)];

  function escapeHtml(s) {
    return s.replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;" }[c]));
  }

  function highlight(code, keywords, preprocessor) {
    const kw = new Set(keywords);
    const re = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/)|("(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')|(\b\d+(?:\.\d*)?(?:[eE][+-]?\d+)?\b)|([A-Za-z_]\w*)|(^[ \t]*#[^\n]*)/gm;
    let html = "";
    let last = 0;
    code.replace(re, (m, com, str, num, word, pre, at) => {
      html += escapeHtml(code.slice(last, at));
      last = at + m.length;
      const cls = com ? "com" : str ? "str" : num ? "num" : pre && preprocessor ? "pre" : word && kw.has(word) ? "kw" : null;
      html += cls ? escapeHtml(m).split("\n").map(part => `<span class="tok-${cls}">${part}</span>`).join("\n") : escapeHtml(m);
      return m;
    });
    html += escapeHtml(code.slice(last));
    return html.split("\n").map(l => `<span class="ln">${l || " "}</span>`).join("");
  }

  function renderCode(el, code, keywords, preprocessor) {
    el.innerHTML = code ? highlight(code.replace(/\n$/, ""), keywords, preprocessor) : "";
  }

  function highlightLine(line) {
    const lines = $("source").querySelectorAll(".ln");
    lines.forEach(l => l.classList.remove("hl"));
    const target = lines[line - 1];
    if (target) {
      target.classList.add("hl");
      target.scrollIntoView({ block: "nearest" });
    }
  }

  function collapseBelow(node, depth) {
    if (node.children) {
      node.children.forEach(c => collapseBelow(c, depth));
      if (node.depth >= depth) {
        node._children = node.children;
        node.children = null;
      }
    }
  }

  function expandAll(node) {
    if (node._children) {
      node.children = node._children;
      node._children = null;
    }
    (node.children || []).forEach(expandAll);
  }

  function shorten(s, max) {
    return s.length > max ? s.slice(0, max - 1) + "…" : s;
  }

  function nodeText(sel) {
    sel.selectAll("tspan").remove();
    sel.append("tspan").attr("class", "role").text(d => d.data.role ? d.data.role + " · " : "");
    sel.append("tspan").attr("class", "kind").attr("fill", color).text(d => d.data.kind);
    sel.append("tspan").text(d => d.data.label ? " " + shorten(d.data.label, 30) : "");
    sel.append("tspan").attr("class", "type").text(d => showTypes && d.data.type ? "  : " + d.data.type : "");
  }

  function diagonal(s, t) {
    return `M${s.y},${s.x}C${(s.y + t.y) / 2},${s.x} ${(s.y + t.y) / 2},${t.x} ${t.y},${t.x}`;
  }

  function update(source) {
    layout(root);
    const nodes = root.descendants();
    const links = root.links();
    const t = svg.transition().duration(DURATION);

    const node = nodeLayer.selectAll("g.node").data(nodes, d => d.id);

    const enter = node.enter().append("g")
      .attr("class", "node")
      .attr("transform", `translate(${source.y0},${source.x0})`)
      .attr("opacity", 0);

    enter.append("circle")
      .attr("r", 5.5)
      .on("click", (e, d) => toggle(d));

    enter.append("text")
      .attr("x", 10)
      .attr("dy", "0.32em")
      .on("click", (e, d) => select(d))
      .on("mouseenter", (e, d) => highlightLine(d.data.line));

    const merged = enter.merge(node);
    merged.classed("selected", d => d === selected);
    merged.select("circle")
      .attr("stroke", color)
      .attr("fill", d => d._children ? color(d) : "#fff");
    merged.select("text").call(nodeText);
    merged.transition(t)
      .attr("transform", d => `translate(${d.y},${d.x})`)
      .attr("opacity", 1);

    node.exit().transition(t)
      .attr("transform", `translate(${source.y},${source.x})`)
      .attr("opacity", 0)
      .remove();

    const link = linkLayer.selectAll("path.link").data(links, d => d.target.id);
    link.enter().append("path")
      .attr("class", "link")
      .attr("d", () => diagonal({ x: source.x0, y: source.y0 }, { x: source.x0, y: source.y0 }))
      .merge(link)
      .transition(t)
      .attr("d", d => diagonal(d.source, d.target));
    link.exit().transition(t)
      .attr("d", () => diagonal(source, source))
      .remove();

    nodes.forEach(d => {
      d.x0 = d.x;
      d.y0 = d.y;
    });
  }

  function toggle(d) {
    if (d.children) {
      d._children = d.children;
      d.children = null;
    } else if (d._children) {
      d.children = d._children;
      d._children = null;
    }
    update(d);
  }

  function countNodes(data) {
    return 1 + (data.children || []).reduce((n, c) => n + countNodes(c), 0);
  }

  function select(d) {
    selected = d;
    highlightLine(d.data.line);
    const count = countNodes(d.data);
    const rows = [
      ["kind", d.data.kind],
      ["label", d.data.label || "—"],
      ["type", d.data.type || "—"],
      ["role", d.data.role || "—"],
      ["line", d.data.line],
      ["depth", d.depth],
      ["subtree nodes", count]
    ];
    const path = d.ancestors().reverse().map(a => a.data.kind).join(" › ");
    $("node-info").innerHTML =
      `<h3 style="color:${color(d)}">${escapeHtml(d.data.kind)} ${escapeHtml(d.data.label || "")}</h3>` +
      `<dl>${rows.map(([k, v]) => `<dt>${k}</dt><dd>${escapeHtml(String(v))}</dd>`).join("")}` +
      `<dt>path</dt><dd>${escapeHtml(path)}</dd></dl>`;
    showTab("node");
    update(d);
  }

  function fit() {
    const box = viewport.node().getBBox();
    const { width, height } = svg.node().getBoundingClientRect();
    if (!box.width || !box.height) return;
    const scale = Math.min(1.2, 0.92 * Math.min(width / box.width, height / box.height));
    const tx = width / 2 - scale * (box.x + box.width / 2);
    const ty = height / 2 - scale * (box.y + box.height / 2);
    svg.transition().duration(DURATION).call(zoom.transform, d3.zoomIdentity.translate(tx, ty).scale(scale));
  }

  function showTab(name) {
    document.querySelectorAll(".tabs button").forEach(b => b.classList.toggle("active", b.dataset.tab === name));
    document.querySelectorAll(".tab").forEach(t => { t.hidden = t.id !== "tab-" + name; });
  }

  function load(data) {
    const bundle = data.ast ? data : { ast: data };
    $("empty").hidden = true;
    $("file-name").textContent = bundle.file || "AST";
    renderCode($("source"), bundle.source || "", C_KEYWORDS, true);
    renderCode($("js"), bundle.js || "// JavaScript is not available for this tree", JS_KEYWORDS, false);
    $("run").disabled = !bundle.js;
    $("output").textContent = "";
    window.CPARSE_DATA = bundle;

    root = d3.hierarchy(bundle.ast, d => d.children);
    root.descendants().forEach(d => { d.id = ++uid; });
    root.x0 = 0;
    root.y0 = 0;
    collapseBelow(root, root.descendants().length > 120 ? 3 : 8);
    selected = null;
    nodeLayer.selectAll("*").remove();
    linkLayer.selectAll("*").remove();
    update(root);
    setTimeout(fit, DURATION + 20);
  }

  function run() {
    const data = window.CPARSE_DATA;
    if (!data || !data.js) return;
    const out = $("output");
    let text = "";
    globalThis.__cparseWrite = s => { text += s; };
    const started = performance.now();
    let status;
    let error = null;
    try {
      status = new Function(data.js + "\nreturn __status;")();
    } catch (e) {
      error = e;
    } finally {
      delete globalThis.__cparseWrite;
    }
    const ms = (performance.now() - started).toFixed(1);
    out.innerHTML = escapeHtml(text) +
      (error ? `<span class="err">${escapeHtml(String(error))}\n</span>` : "") +
      `<span class="meta">— exit code ${error ? 1 : status}, ${ms} ms</span>`;
  }

  function openFile(file) {
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const text = String(reader.result).replace(/^window\.CPARSE_DATA\s*=\s*/, "").replace(/;\s*$/, "");
        load(JSON.parse(text));
      } catch (e) {
        alert("Cannot read " + file.name + ": " + e.message);
      }
    };
    reader.readAsText(file);
  }

  $("expand").onclick = () => { if (root) { expandAll(root); update(root); setTimeout(fit, DURATION + 20); } };
  $("collapse").onclick = () => { if (root) { expandAll(root); collapseBelow(root, 2); update(root); setTimeout(fit, DURATION + 20); } };
  $("fit").onclick = fit;
  $("show-types").onchange = e => { showTypes = e.target.checked; if (root) update(root); };
  $("open").onchange = e => { if (e.target.files[0]) openFile(e.target.files[0]); };
  $("run").onclick = run;
  document.querySelectorAll(".tabs button").forEach(b => { b.onclick = () => showTab(b.dataset.tab); });

  const pane = $("tree-pane");
  pane.addEventListener("dragover", e => e.preventDefault());
  pane.addEventListener("drop", e => {
    e.preventDefault();
    if (e.dataTransfer.files[0]) openFile(e.dataTransfer.files[0]);
  });

  if (window.CPARSE_DATA) load(window.CPARSE_DATA);
  else $("empty").hidden = false;
})();
