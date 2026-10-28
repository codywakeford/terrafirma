// The sweep's measures against the goldens: every number, exactly (the
// atlas is deterministic), so any change to the mapping shows up here.
// `--update` writes the current sweep as the new golden.
const [latitude, cylinder, flag] = process.argv.slice(2);
const goldenPath = `${import.meta.dir}/golden.json`;

type Row = Record<string, unknown>;
const pick = (s: any): Row => ({
  radius: s.params.radius,
  shells: s.shells.length,
  bands: s.surface.bands,
  equatorBand: s.surface.equatorBand,
  regular: s.surface.regular,
  withinTolerance: s.surface.withinTolerance,
  worst: s.worst,
  cap: s.cap,
  rail: s.rails.northSouth[0],
});

const now = {
  latitude: (await Bun.file(latitude).json()).map(pick),
  cylinder: (await Bun.file(cylinder).json()).map(pick),
};

if (flag === "--update") {
  await Bun.write(goldenPath, JSON.stringify(now, null, 2) + "\n");
  console.log(`atlas-check: golden updated (${goldenPath})`);
  process.exit(0);
}
if (flag !== undefined) throw new Error(`unknown flag ${flag}`);

const golden = await Bun.file(goldenPath).json();
const differences: string[] = [];
const walk = (a: unknown, b: unknown, path: string) => {
  if (typeof a === "object" && a !== null && typeof b === "object" && b !== null) {
    for (const k of new Set([...Object.keys(a), ...Object.keys(b)]))
      walk((a as Row)[k], (b as Row)[k], `${path}.${k}`);
  } else if (a !== b) differences.push(`${path}: golden ${JSON.stringify(a)}, now ${JSON.stringify(b)}`);
};
walk(golden, now, "sweep");

if (differences.length) {
  console.error(`atlas-check: ${differences.length} differences from the golden mapping:`);
  for (const d of differences.slice(0, 40)) console.error(`  ${d}`);
  process.exit(1);
}
console.log(`atlas-check: ${now.latitude.length + now.cylinder.length} planets match the golden mapping`);
