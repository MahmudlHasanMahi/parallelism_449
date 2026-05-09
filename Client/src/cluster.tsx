// cluster.tsx

export const THREADS = 1;
export const CLUSTER_RADIUS = 8000;
export const CLUSTER_SPREAD = 20000;

const CLUSTER_COLORS = [
  "#ff6644",
  "#72bd72",
  "#ffff44",
  "#ed7171",
  "#44fff3",
  "#bf67ba",
];

export const clusterCenters = Array.from({ length: THREADS }, (_, i) => {
  const angle = (i / THREADS) * 2 * Math.PI;
  return {
    x: Math.cos(angle) * CLUSTER_SPREAD,
    y: Math.sin(angle) * CLUSTER_SPREAD,
  };
});

export const clusterColor = (thread: number): string => {
  return CLUSTER_COLORS[thread % CLUSTER_COLORS.length];
};

export const getNodePosition = (id: number) => {
  const thread = id % THREADS;
  const center = clusterCenters[thread];
  const angle = Math.random() * 2 * Math.PI;
  const radius = Math.sqrt(Math.random()) * CLUSTER_RADIUS;

  return {
    x: center.x + Math.cos(angle) * radius,
    y: center.y + Math.sin(angle) * radius,
    color: clusterColor(thread),
  };
};
