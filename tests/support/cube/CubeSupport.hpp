#pragma once

/**
 * What the tests of every layer share, at the cube's own level.
 *
 * The tests keep the walls the engine has. A helper lives in the directory of
 * the lowest layer that can see everything it uses, and a test executable is
 * given only the directories its own library may see -- so a cube test that
 * reaches for a graphics helper fails to find the header, the same way cube
 * itself fails to find graphics.
 *
 * This one includes nothing but cube/ and the standard library, which is what
 * lets tests/cube and tests/solver use it as readily as the layers above them.
 *
 * No Catch2 in here: the benches link without it, and a helper that asserts
 * belongs beside the tests that assert.
 */
namespace rubiks::test {

}  // namespace rubiks::test
