/**************************************************************************/
/*  test_e25_provenance_reuse.cpp                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_types.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_e25_provenance_reuse)

TEST_CASE("[E25] surface conversion drops stale index provenance on reuse") {
	RenderingServer *server = RenderingServer::get_singleton();
	CHECK(server != nullptr);
	if (!server) {
		return;
	}
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	PackedVector3Array vertices;
	vertices.push_back(Vector3(0, 0, 0));
	vertices.push_back(Vector3(1, 0, 0));
	vertices.push_back(Vector3(0, 1, 0));
	arrays[RSE::ARRAY_VERTEX] = vertices;
	PackedInt32Array indices;
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);
	arrays[RSE::ARRAY_INDEX] = indices;

	RenderingServerTypes::SurfaceData surface;
	surface.index_provenance_schema = 1;
	surface.index_provenance_kind = RenderingServerTypes::SurfaceData::IndexProvenanceKind::TYPED_SIGNED32;
	surface.index_provenance_word_count = 3;
	surface.index_provenance_words.push_back(9);
	surface.index_provenance_words.push_back(8);
	surface.index_provenance_words.push_back(7);
	const Error indexed = server->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), 0);
	CHECK(indexed == OK);
	if (indexed != OK) {
		return;
	}
	CHECK(surface.index_count == 3);
	CHECK(surface.index_provenance_schema == 0);
	CHECK(surface.index_provenance_kind == RenderingServerTypes::SurfaceData::IndexProvenanceKind::NONE);
	CHECK(surface.index_provenance_word_count == 0);
	CHECK(surface.index_provenance_words.is_empty());

	surface.index_provenance_schema = 1;
	surface.index_provenance_kind = RenderingServerTypes::SurfaceData::IndexProvenanceKind::TYPED_SIGNED32;
	surface.index_provenance_word_count = 1;
	surface.index_provenance_words.push_back(42);
	ERR_PRINT_OFF;
	const Error invalid = server->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_TRIANGLES, Array(), Array(), Dictionary(), 0);
	ERR_PRINT_ON;
	CHECK(invalid != OK);
	CHECK(surface.index_provenance_schema == 1);
	CHECK(surface.index_provenance_kind == RenderingServerTypes::SurfaceData::IndexProvenanceKind::TYPED_SIGNED32);
	CHECK(surface.index_provenance_word_count == 1);
	CHECK(surface.index_provenance_words.size() == 1);
	if (surface.index_provenance_words.size() == 1) {
		CHECK(surface.index_provenance_words[0] == 42);
	}

	arrays[RSE::ARRAY_INDEX] = Variant();
	const Error unindexed = server->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), 0);
	CHECK(unindexed == OK);
	if (unindexed != OK) {
		return;
	}
	CHECK(surface.index_count == 0);
	CHECK(surface.index_provenance_schema == 0);
	CHECK(surface.index_provenance_kind == RenderingServerTypes::SurfaceData::IndexProvenanceKind::NONE);
	CHECK(surface.index_provenance_word_count == 0);
	CHECK(surface.index_provenance_words.is_empty());
}
