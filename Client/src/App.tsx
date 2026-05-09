import { useEffect, useRef, useState } from "react";
import Sigma from "sigma";
import Graph from "graphology";
import FA2Layout from "graphology-layout-forceatlas2/worker";
import circular from "graphology-layout/circular";
import {
  getNodePosition,
  THREADS,
  clusterCenters,
  clusterColor,
} from "./cluster";

import style from "./style.module.css";
export default function App() {
  const graphRef = useRef<Graph | null>(null);
  const sigmaRef = useRef<Sigma | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const updateQueueRef = useRef<
    Array<{ nodeId: number; oldParent: number; newParent: number }>
  >([]);
  const stopRef = useRef(false);
  const fa2Ref = useRef<FA2Layout | null>(null);
  const [metrics, useMetrics] = useState({
    threads: 1,
    total_edges: 0,
    total_nodes: 0,
    nodes_explored: 0,
    buffer_size: 0,
    source: -1,
    destination: -1,
    found: false,
    best_cost: -1,
  });

  const oldedge = useRef<any[]>([]);
  const isProcessingRef = useRef(false);
  const BATCH_SIZE = 2000; // How many edges per tick
  const INTERVAL_MS = 4000;
  const MAX_EDGES = 500;
  const SKIP_PERCENT = 0.95;

  // ✅ Drain queue slowly
  const processQueue = () => {
    if (isProcessingRef.current) return;
    isProcessingRef.current = true;

    const graph = graphRef.current;
    // How often to process (ms) — increase to slow dow
    const tick = () => {
      if (!graph || stopRef.current) {
        // setTimeout(tick, INTERVAL_MS);
        return;
      }

      const batch = updateQueueRef.current.splice(0, BATCH_SIZE);

      if (batch.length === 0) {
        // ✅ Keep ticking, don't stop
        // isProcessingRef.current = false; // ← Remove this line
        // setTimeout(tick, INTERVAL_MS);
        return;
      }
      let i = 0;
      for (const { nodeId, oldParent, newParent } of batch) {
        i++;
        if (oldParent !== -1) {
          try {
            const edges = graph.edges(String(oldParent), String(nodeId));
            graph.dropEdge(edges);
          } catch (e) {}
        }

        if (newParent !== -1) {
          const delay = (INTERVAL_MS / BATCH_SIZE) * i;

          let edgeu = String(newParent);
          let edgev = String(nodeId);

          setTimeout(() => {
            if (!graph.hasEdge(edgeu, edgev)) {
              const edge = graph.addEdge(edgeu, edgev, {
                size: 0.000005,
                color: "#437a7b",
              });

              setTimeout(() => {
                if (graph.hasEdge(edge)) {
                  graph.setEdgeAttribute(edge, "color", "rgb(227, 227, 227)");
                }
              }, delay / 3);
            }
          }, delay);

          graph.setNodeAttribute(String(edgev), "color", "#3823f5");
          graph.setNodeAttribute(String(edgeu), "color", "#3823f5");
        }
      }

      // ✅ Always schedule next tick
      setTimeout(() => {
        graph.clearEdges();
      }, INTERVAL_MS / 2);
      setTimeout(tick, INTERVAL_MS);
    };

    tick();
  };
  useEffect(() => {
    if (containerRef.current) {
      containerRef.current.style.width = window.innerWidth + "px";
      containerRef.current.style.height = window.innerHeight + "px";

      graphRef.current = new Graph();
      sigmaRef.current = new Sigma(graphRef.current, containerRef.current, {
        renderEdgeLabels: false,
        defaultNodeColor: "#a39f9f",
        defaultEdgeColor: "#322e2e",
      });

      sigmaRef.current.resize();
    }
    return () => {
      fa2Ref.current?.stop(); // ✅ Stop layout on unmount
      sigmaRef.current?.kill();
    };
  }, []);
  const parseNode = (arrayBuffer: ArrayBuffer) => {
    sigmaRef.current?.refresh();
    sigmaRef.current?.getCamera().setState({
      x: 1,
      y: 0.32,
      ratio: 1.4, // Zoom out to see everything
    });
    const view = new DataView(arrayBuffer);
    let offset = 0;
    const graph = graphRef.current;
    if (!graph) return;

    while (offset < view.byteLength) {
      const val = view.getInt32(offset, true);
      offset += 4;
      if (val === -1 || val === -2) continue;

      const u = val;
      if (offset + 4 > view.byteLength) return;
      const totalChildren = view.getInt32(offset, true);
      offset += 4;

      const children: number[] = [];
      for (let i = 0; i < totalChildren; i++) {
        if (offset >= view.byteLength) break;
        const child = view.getInt32(offset, true);
        offset += 4;
        if (child === -1) break;
        if (child === -2) continue;
        children.push(child);
      }
      if (!graph.hasNode(String(u))) {
        graph.addNode(String(u), {
          ...getNodePosition(u),
          size: 1,
          label: String(u),
        });
      }

      for (const c of children) {
        if (!graph.hasNode(String(c))) {
          graph.addNode(String(c), {
            ...getNodePosition(c),
            size: 1,
            label: String(c),
          });
        }
      }
    }

    console.log("✅ Graph loaded:", graph.order, "nodes", graph.size, "edges");
  };
  //
  const parseEdgeUpdates = (arrayBuffer: ArrayBuffer) => {
    const view = new DataView(arrayBuffer);
    let offset = 0;
    const graph = graphRef.current;
    if (!graph) return;

    const marker = view.getInt32(offset, true);
    offset += 4;
    if (marker !== -4) return;

    const count = view.getInt32(offset, true);
    offset += 4;

    for (let i = 0; i < count; i++) {
      if (offset + 12 > view.byteLength) break;

      const nodeId = view.getInt32(offset, true);
      offset += 4;
      const oldParent = view.getInt32(offset, true);
      offset += 4;
      const newParent = view.getInt32(offset, true);
      offset += 4;

      if (Math.random() > SKIP_PERCENT) {
        updateQueueRef.current.push({ nodeId, oldParent, newParent });
      }
    }

    processQueue();
  };

  const parseFinalPath = (arrayBuffer: ArrayBuffer) => {
    const view = new DataView(arrayBuffer);
    let offset = 0;
    const graph = graphRef.current;
    if (!graph) return;

    const marker = view.getInt32(offset, true);
    offset += 4;
    if (marker !== -5) return;

    const count = view.getInt32(offset, true);
    offset += 4;

    graph.clearEdges();

    for (let i = 0; i < count; i++) {
      if (offset + 12 > view.byteLength) break;

      const nodeId = view.getInt32(offset, true);
      offset += 4;
      const oldParent = view.getInt32(offset, true);
      offset += 4;
      const newParent = view.getInt32(offset, true);
      offset += 4;

      if (newParent !== -1) {
        if (!graph.hasNode(String(newParent))) {
          graph.addNode(String(newParent), {
            x: Math.random() * 100,
            y: Math.random() * 100,
            size: 5,
            color: "#000000",
          });
        }
        if (!graph.hasNode(String(nodeId))) {
          graph.addNode(String(nodeId), {
            x: Math.random() * 100,
            y: Math.random() * 100,
            size: 5,
            color: "#000000",
          });
        }
        graph.mergeEdge(String(newParent), String(nodeId), {
          size: 2,
          color: "#000000",
        });
        if (nodeId === 1) {
          graph.setNodeAttribute(String(nodeId), "color", "#ff00cc");
        } else if (nodeId === 41189) {
          graph.setNodeAttribute(String(nodeId), "color", "#0033ff");
        } else {
          graph.setNodeAttribute(String(nodeId), "color", "#00ff00");
        }
        graph.setNodeAttribute(String(nodeId), "size", 5);
        graph.setNodeAttribute(String(newParent), "color", "#00ff00");
        graph.setNodeAttribute(String(newParent), "size", 5);
      }
    }

    console.log("✅ Final path highlighted");
  };

  const startStreaming = () => {
    if (wsRef.current) return;
    const ws = new WebSocket("ws://localhost:9001/ws");
    ws.binaryType = "arraybuffer";

    ws.onopen = () => console.log("✅ Connected");
    ws.onmessage = (event) => {
      if (typeof event.data === "string") {
        const data = JSON.parse(event.data);
        if (data.type === "metrics") {
          useMetrics((prev) => ({
            ...prev,
            buffer_size: data.buffer_size,
            nodes_explored: data.nodes_explored,
            best_cost: data.best_cost,
          }));
        } else {
          useMetrics((prev) => ({
            ...prev,
            total_edges: data.total_edges,
            total_nodes: data.total_nodes,
            source: data.source_node,
            destination: data.destination_node,
            threads: data.threads,
          }));
        }
      } else {
        const view = new DataView(event.data);
        const firstMarker = view.getInt32(0, true);

        if (firstMarker === -4) {
          parseEdgeUpdates(event.data); // 🔴 Live updates
        } else if (firstMarker === -5) {
          const data = event.data;
          const wait = () => {
            if (updateQueueRef.current.length === 0) {
              useMetrics((prev) => ({
                ...prev,
                found: true,
              }));
              parseFinalPath(data);
              useMetrics((prev) => ({ ...prev, found: true }));
            } else {
              setTimeout(
                wait,
                (INTERVAL_MS * INTERVAL_MS * SKIP_PERCENT * 2.2) / BATCH_SIZE,
              ); // ✅ Same interval as tick
            }
          };
          wait(); // 🟢 Final path
        } else if (firstMarker === -6) {
          // ✅ No path found
          stopRef.current = true; // ✅ Stop the queue
          updateQueueRef.current = []; // ✅ Clear pending updates
          graphRef.current?.clearEdges(); // ✅ Clear edges
          useMetrics((prev) => ({ ...prev, path_not_found: true }));
          return;
        } else {
          parseNode(event.data); // ⚪ Initial graph
        }
      }
    };

    ws.onerror = (err) => console.error("❌ Error:", err);
    ws.onclose = () => {
      console.log("🔌 Disconnected");
      wsRef.current = null;
    };

    wsRef.current = ws;
  };

  return (
    <div className={style.wrapper}>
      <button onClick={startStreaming} className={style.btn}>
        Connect
      </button>

      <div className={style.metric_wrapper}>
        <span>Threads: {metrics?.threads}</span>
        <span>total edges: {metrics.total_edges.toLocaleString()}</span>
        <span>total nodes: {metrics.total_nodes.toLocaleString()}</span>
        <span>Source Node: {metrics.source}</span>
        <span>Destination Node: {metrics.destination}</span>
        {metrics.found && (
          <>
            <span>
              Node Explored: {metrics.nodes_explored.toLocaleString()}
              <br />
              Best Cost: {metrics.best_cost}
            </span>
          </>
        )}
        {metrics?.path_not_found && (
          <>
            <span style={{ color: "red" }}>Path not found</span>
          </>
        )}
      </div>
      <div ref={containerRef} style={{ width: "100%", height: "100%" }} />
    </div>
  );
}
