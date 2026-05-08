import { useEffect, useRef } from "react";
import Sigma from "sigma";
import Graph from "graphology";

export default function App() {
  const graphRef = useRef<Graph | null>(null);
  const sigmaRef = useRef<Sigma | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const updateQueueRef = useRef<
    Array<{ nodeId: number; oldParent: number; newParent: number }>
  >([]);
  const isProcessingRef = useRef(false);

  // ✅ Drain queue slowly
  const processQueue = () => {
    if (isProcessingRef.current) return;
    isProcessingRef.current = true;

    const BATCH_SIZE = 1; // How many edges per tick
    const INTERVAL_MS = 1000; // How often to process (ms) — increase to slow down
    const tick = () => {
      const graph = graphRef.current;
      if (!graph) return;

      const batch = updateQueueRef.current.splice(0, BATCH_SIZE);
      if (batch.length === 0) {
        isProcessingRef.current = false;
        return;
      }
      for (const { nodeId, oldParent, newParent } of batch) {
        // Remove old edge
        if (oldParent !== -1) {
          console.log("dropping");
          try {
            const edges = graph.edges(String(oldParent), String(nodeId));
            edges.forEach((edge) => graph.dropEdge(edge));
          } catch (e) {}
        }

        // Add new edge
        if (newParent !== -1) {
          if (!graph.hasNode(String(newParent))) {
            graph.addNode(String(newParent), {
              x: Math.random() * 100,
              y: Math.random() * 100,
              size: 5,
              color: "#169f34",
            });
          }
          if (!graph.hasNode(String(nodeId))) {
            graph.addNode(String(nodeId), {
              x: Math.random() * 100,
              y: Math.random() * 100,
              size: 5,
              color: "#169f34",
            });
          }
          if (!graph.hasEdge(String(newParent), String(nodeId))) {
            graph.addEdge(String(newParent), String(nodeId), {
              size: 1,
              color: "#ff0000",
            });
          }
          graph.setNodeAttribute(String(nodeId), "color", "#f523dd");
          graph.setNodeAttribute(String(nodeId), "size", 5);
          graph.setNodeAttribute(String(newParent), "color", "#f523dd");
          graph.setNodeAttribute(String(newParent), "size", 5);
        }
      }

      setTimeout(tick, INTERVAL_MS);
    };

    tick();
  };
  useEffect(() => {
    if (containerRef.current) {
      graphRef.current = new Graph();
      sigmaRef.current = new Sigma(graphRef.current, containerRef.current, {
        renderEdgeLabels: false,
        defaultNodeColor: "#169f34",
        defaultEdgeColor: "#322e2e",
      });
    }
    return () => {
      sigmaRef.current?.kill();
    };
  }, []);

  const parseNode = (arrayBuffer: ArrayBuffer) => {
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
          x: Math.random() * 100,
          y: Math.random() * 100,
          size: 5,
          label: String(u),
        });
      }

      for (const c of children) {
        if (!graph.hasNode(String(c))) {
          graph.addNode(String(c), {
            x: Math.random() * 100,
            y: Math.random() * 100,
            size: 5,
            label: String(c),
          });
          setTimeout(() => {}, 1000);
        }
        // if (!graph.hasEdge(String(u), String(c))) {
        //   graph.addEdge(String(u), String(c));
        // }
      }
    }

    console.log("✅ Graph loaded:", graph.order, "nodes", graph.size, "edges");
  };

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

      // ✅ Just push to queue, don't process immediately
      const SKIP_PERCENT = 0.4; // skip 80%
      if (Math.random() > SKIP_PERCENT) {
        updateQueueRef.current.push({ nodeId, oldParent, newParent });
      }
    }

    // Start draining if not already
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

    // ✅ Clear all explored edges first
    // graph.clearEdges();

    // ✅ Reset all nodes to default color
    graph.forEachNode((node) => {
      graph.setNodeAttribute(node, "color", "#169f34");
      graph.setNodeAttribute(node, "size", 1);
    });

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
            color: "#00ff00",
          });
        }
        if (!graph.hasNode(String(nodeId))) {
          graph.addNode(String(nodeId), {
            x: Math.random() * 100,
            y: Math.random() * 100,
            size: 5,
            color: "#00ff00",
          });
        }
        graph.mergeEdge(String(newParent), String(nodeId), {
          size: 2,
          color: "#00ff00",
        });
        graph.setNodeAttribute(String(nodeId), "color", "#00ff00");
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
        console.log("Meta:", JSON.parse(event.data));
      } else {
        const view = new DataView(event.data);
        const firstMarker = view.getInt32(0, true);

        if (firstMarker === -4) {
          parseEdgeUpdates(event.data); // 🔴 Live updates
        } else if (firstMarker === -5) {
          // parseFinalPath(event.data); // 🟢 Final path
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
    <div>
      <button onClick={startStreaming}>Connect</button>
      <div ref={containerRef} style={{ width: "100vw", height: "100vh" }} />
    </div>
  );
}
