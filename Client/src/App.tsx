import React, { useState, useRef } from "react";
import { Cosmograph, CosmographProvider } from "@cosmograph/react";
import type { CosmographConfig, CosmographRef } from "@cosmograph/react";
import { faker } from "@faker-js/faker";
import s from "./style.module.css";
import { PointShape } from "@cosmos.gl/graph";

export const title = "Adding and removing data items";
export const subTitle = "Histogram reacting to data changes";
export const category = "Data handling";

const config: CosmographConfig = {
  linkColor: [59, 74, 97, 1],
  pointDefaultSize: 2,
  points: [
    { id: "1", index: 0, color: faker.color.rgb() },
    { id: "2", index: 1, color: faker.color.rgb() },
    { id: "3", index: 2, color: faker.color.rgb() },
  ],
  links: [],
  // pointColorBy: "color",
  pointIdBy: "id",
  pointIndexBy: "index",
  // linkSourceBy: "source",
  // linkTargetBy: "target",
  linkSourceIndexBy: "sourceidx",
  linkTargetIndexBy: "targetidx",
  // pointColorStrategy: "direct",
  showDynamicLabels: true,
  enableSimulation: false,
  rescalePositions: false,
  // pointSizeScale: 1,
};

function App() {
  const cosmograph = useRef<CosmographRef>(null);
  const [stats, setStats] = useState({
    pointsCount: 0,
    linksCount: 0,
  });
  const addPoint = async (): Promise<void> => {
    const newPoint = {
      id: `${stats.pointsCount + 1}`,
    };
    await cosmograph.current?.addPoints([newPoint]);
    cosmograph.current?.fitView();
  };

  const addMultiplePoints = async (count: number = 10): Promise<void> => {
    const newPoints = Array.from({ length: count }, (_, i) => ({
      id: `${stats.pointsCount + i + 1}`,
      
      color: faker.color.rgb(),
    }));

    await cosmograph.current?.addPoints(newPoints);
    // cosmograph.current?.setFocusedPoint(0);
  };

  const removeMultiplePoints = async (count: number = 10): Promise<void> => {
    const indicesToRemove = Array.from(
      { length: count },
      (_, i) => stats.pointsCount - 1 - i,
    );
    await cosmograph.current?.removePointsByIndices(indicesToRemove);
    cosmograph.current?.fitView();
  };

  const removeLastPoint = async (): Promise<void> => {
    await cosmograph.current?.removePointsByIndices(
      [stats.pointsCount - 1],
      true,
    );
    cosmograph.current?.fitView();
  };

  const addNextLink = async (): Promise<void> => {
    const link = {
      source: (stats.linksCount + 1).toString(),
      target: (stats.linksCount + 2).toString(),
    };
    await cosmograph.current?.addLinks([link]);
  };

  const removeLastLink = async (): Promise<void> => {
    if (stats.linksCount > 0) {
      const lastLinkSource = stats.linksCount.toString();
      const lastLinkTarget = (stats.linksCount + 1).toString();
      await cosmograph.current?.removeLinksByPointIdPairs([
        [lastLinkSource, lastLinkTarget],
      ]);
    }
  };

  return (
    <div className={s.wrapper}>
      <CosmographProvider>
        <Cosmograph
          preservePointPositionsOnDataUpdate={true}
          ref={cosmograph}
          className={s.graph}
          {...config}
          onGraphRebuilt={setStats}
        />
      </CosmographProvider>
      <div className={s.props}>
        <button onClick={addPoint}>add point</button>
        <button onClick={removeLastPoint}>remove last point</button>
        <button onClick={addNextLink}>add next link</button>
        <button onClick={removeLastLink}>remove last link</button>
        <button
          onClick={() => {
            addMultiplePoints(5);
          }}
        >
          add 10 points
        </button>
        <button onClick={() => removeMultiplePoints(10)}>
          remove last 10 points
        </button>
        <div style={{ fontSize: "12px" }}>points: {stats.pointsCount}</div>
        <div style={{ fontSize: "12px" }}>links: {stats.linksCount}</div>
      </div>
    </div>
  );
}

export default App;
