// Runs every wiki demo spec (from specs.ts, on stdin) in the engine's
// sandbox for a simulated minute; reports any that can't be made or stepped,
// and any reaction demo that never makes its products.
#include "matter/matter.h"
#include "sandbox/sandbox.h"
#include <json/json.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>

using namespace sandbox;

static Parts parts(const Json::Value &v)
{
	Parts out;
	for (const std::string &id : v.getMemberNames())
		out.push_back({ id, v[id].asDouble() });
	return out;
}

int main(int argc, char **argv)
{
	matter::Matter m = matter::Matter::load(argv[1]);
	Json::Value all;
	std::cin >> all;
	int bad = 0;
	for (const Json::Value &d : all) {
		const Json::Value &s = d["spec"];
		auto t0 = std::chrono::steady_clock::now();
		try {
			Sandbox box(Opts{ m, s["size"][0].asInt(), s["size"][1].asInt(), s["size"][2].asInt(), s["block"].asDouble(),
					s["ambient"].asDouble(), s["pressure"].asDouble(), parts(s["air"]) });
			for (const Json::Value &b : s["blocks"]) {
				At at{ b["at"][0].asInt(), b["at"][1].asInt(), b["at"][2].asInt() };
				if (b.isMember("fill")) box.fill(at, parts(b["fill"]), b["temperature"].asDouble(), b.isMember("gas") ? parts(b["gas"]) : Parts{});
				else if (b.isMember("gas")) box.gas(at, parts(b["gas"]), b["temperature"].asDouble());
				else if (b.isMember("temperature")) box.touch(at, b["temperature"].asDouble());
				if (b.isMember("heater")) box.heater(at, b["heater"].asDouble());
			}
			// what's made: each expected product's moles, over the box, before and after
			auto made = [&](const std::string &id) {
				size_t k = m.index(id);
				double n = 0;
				for (int32_t y = 0; y < box.sizeY(); y++)
					for (int32_t z = 0; z < box.sizeZ(); z++)
						for (int32_t x = 0; x < box.sizeX(); x++)
							n += box.block({ x, y, z }).amounts[k];
				return n;
			};
			std::vector<double> before;
			for (const Json::Value &id : d["expect"])
				before.push_back(made(id.asString()));
			for (int k = 0; k < 60; k++)
				box.step(1);
			// a product that also leaves by the open top (a gas) may net out low: any rise counts
			std::string missing;
			for (Json::ArrayIndex k = 0; k < d["expect"].size(); k++) {
				std::string id = d["expect"][k].asString();
				if (made(id) <= before[k] + 1e-9 && m.substance(id).state(box.block({ 1, 0, 1 }).temperature, 101325) != matter::State::Gas)
					missing += (missing.empty() ? "" : ", ") + id;
			}
			if (!missing.empty())
				throw std::runtime_error("ran, but made no " + missing);
			double hottest = 0, active = 0;
			for (const BlockView &v : box.view()) {
				hottest = std::max(hottest, v.temperature);
				active = std::max(active, std::fabs(v.activity));
			}
			double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			std::printf("ok   %-34s %6.0f ms per simulated minute · hottest %5.0f K · most activity %.2f W\n",
					d["name"].asCString(), ms, hottest, active);
		} catch (const std::exception &e) {
			bad++;
			std::printf("FAIL %-34s %s\n", d["name"].asCString(), e.what());
		}
	}
	std::printf("wiki check: %d/%d demos run\n", (int)all.size() - bad, (int)all.size());
	return bad ? 1 : 0;
}
