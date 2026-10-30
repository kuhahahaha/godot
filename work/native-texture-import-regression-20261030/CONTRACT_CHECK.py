from pathlib import Path

root = Path(__file__).resolve().parents[2]
rd = (root / 'servers/rendering/renderer_rd/storage_rd/texture_storage.cpp').read_text()
common = (root / 'servers/rendering/storage/texture_storage.h').read_text()
rd_h = (root / 'servers/rendering/renderer_rd/storage_rd/texture_storage.h').read_text()
gles_h = (root / 'drivers/gles3/storage/texture_storage.h').read_text()
dummy_h = (root / 'servers/rendering/dummy/storage/texture_storage.h').read_text()
gles = (root / 'drivers/gles3/storage/texture_storage.cpp').read_text()
checks = {
    'common_bool_contract': 'virtual bool texture_rd_initialize' in common,
    'rd_bool_contract': 'virtual bool texture_rd_initialize' in rd_h,
    'gles_bool_contract': 'virtual bool texture_rd_initialize' in gles_h and 'bool TextureStorage::texture_rd_initialize' in gles,
    'dummy_bool_contract': 'virtual bool texture_rd_initialize' in dummy_h,
    'caller_checks_status': 'if (!texture_rd_initialize(texture, rd_texture, p_layered_type))' in rd,
    'reserved_rid_cleanup': 'texture_owner.initialize_rid(texture, Texture());' in rd and 'texture_owner.free(texture);' in rd,
    'source_rid_cleanup': 'RD::get_singleton()->free_rid(rd_texture);' in rd,
    'invalid_source_rejected': 'texture_is_valid(p_rd_texture), false' in rd,
    'sampling_usage_rejected': 'TEXTURE_USAGE_SAMPLING_BIT), false' in rd,
    'invalid_image_format_rejected': 'imfmt.image_format == Image::FORMAT_MAX, false' in rd,
    'invalid_type_layers_rejected': 'tf.array_layers != 1, false' in rd and 'tf.array_layers == 1, false' in rd,
    'primary_shared_failure': 'if (texture.rd_texture.is_null())' in rd,
    'srgb_shared_failure': 'if (texture.rd_texture_srgb.is_null())' in rd and 'free_rid(texture.rd_texture);' in rd,
    'success_published_last': 'texture_owner.initialize_rid(p_texture, texture);\n\treturn true;' in rd,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f'{name}={int(ok)}')
if failed:
    raise SystemExit('failed: ' + ','.join(failed))
