// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "workshop/library.h"
#include "exceptions.h"
#include "filesys.h"

using namespace workshop;

class TestLibrary : public TestBase {
public:
	TestLibrary() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestLibrary"; }

	void runTests(IGameDef *gamedef);

	void testBlueprintWrittenAndRead();
	void testKeptByPlayerAndName();
};

static TestLibrary g_test_instance;

void TestLibrary::runTests(IGameDef *gamedef)
{
	TEST(testBlueprintWrittenAndRead);
	TEST(testKeptByPlayerAndName);
}

// A blueprint's machines and where each goes survive its text; text that
// isn't one, or one of none, throws.
void TestLibrary::testBlueprintWrittenAndRead()
{
	const Blueprint b{ { { v3s16(0, 0, 0), std::string("one\0piece", 9) }, { v3s16(-2, 3, 1), "two" } } };
	const Blueprint back = Blueprint::read(b.write());
	UASSERTEQ(size_t, back.parts.size(), 2);
	UASSERT(back.parts[0].machine == b.parts[0].machine && back.parts[1].offset == v3s16(-2, 3, 1));
	EXCEPTION_CHECK(SerializationError, Blueprint::read("TFP2 not a blueprint"));
	EXCEPTION_CHECK(SerializationError, Blueprint::read(Blueprint{}.write()));
}

// Each player's own, by name, sorted; saved again under a name, it's the
// new one. A name that isn't one is refused, and an unknown one isn't there.
void TestLibrary::testKeptByPlayerAndName()
{
	const std::string world = getTestTempDirectory();
	fs::RecursiveDelete(world + DIR_DELIM + "blueprints");
	Library lib(world);
	const Blueprint cart{ { { v3s16(0), "cart" } } }, pot{ { { v3s16(0), "pot" } } };
	lib.save("cody", "small cart", cart);
	lib.save("cody", "Pot_2", pot);
	lib.save("ann", "pot", pot);
	UASSERT(lib.names("cody") == std::vector<std::string>({ "Pot_2", "small cart" }));
	UASSERT(lib.names("ann") == std::vector<std::string>({ "pot" }));
	UASSERT(lib.names("nobody").empty());
	UASSERT(lib.load("cody", "small cart").parts[0].machine == "cart");
	lib.save("cody", "small cart", pot);
	UASSERT(lib.load("cody", "small cart").parts[0].machine == "pot");

	for (const char *bad : { "", " pad", "../ann/pot", "a/b", "dot.dot", "012345678901234567890123456789012" }) {
		UASSERT(!Library::named(bad));
		EXCEPTION_CHECK(std::invalid_argument, lib.save("cody", bad, cart));
	}
	EXCEPTION_CHECK(std::out_of_range, lib.load("cody", "pot"));
	EXCEPTION_CHECK(std::out_of_range, lib.load("cody", "../ann/pot"));
}
