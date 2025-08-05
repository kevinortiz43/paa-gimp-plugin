#include "paa-gimp-plugin.h"

G_DEFINE_TYPE (PAA, paa, GIMP_TYPE_PLUG_IN)

static GList * paa_query_procedures(GimpPlugIn *plug_in)
{
    return g_list_append(g_list_append(NULL, g_strdup(LOAD_PROC)), g_strdup(SAVE_PROC));
}

static GimpProcedure* paa_create_procedure(GimpPlugIn *plugin, const gchar *name)
{
    GimpProcedure *procedure = NULL;

    if (!strcmp(name, LOAD_PROC)) {
        procedure = gimp_load_procedure_new(plugin, name, GIMP_PDB_PROC_TYPE_PLUGIN, paa_load, NULL, NULL);
        gimp_file_procedure_set_magics(GIMP_FILE_PROCEDURE(procedure), "0,leshort,65281,0,leshort,65285");
        gimp_file_procedure_set_format_name(GIMP_FILE_PROCEDURE(procedure), "PAA");
        gimp_file_procedure_set_extensions(GIMP_FILE_PROCEDURE (procedure), "paa");
    } else if (!strcmp(name, SAVE_PROC)) {
        procedure = gimp_export_procedure_new(plugin, name, GIMP_PDB_PROC_TYPE_PLUGIN, false, paa_save, NULL, NULL);
        gimp_procedure_set_image_types (procedure, "RGB*");
        gimp_procedure_set_menu_label (procedure, "PAA Image");
        gimp_file_procedure_set_format_name(GIMP_FILE_PROCEDURE(procedure), "PAA");
        gimp_file_procedure_set_extensions(GIMP_FILE_PROCEDURE (procedure), "paa");
        gimp_export_procedure_set_capabilities(GIMP_EXPORT_PROCEDURE (procedure),
                                               GimpExportCapabilities(GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_ALPHA),
                                               NULL, NULL, NULL);
    }

    gimp_procedure_set_attribution(procedure, "Gruppe Adler", "Gruppe Adler", "2020");

    return procedure;
}

static void paa_class_init(PAAClass *klass)
{
    GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

    plug_in_class->query_procedures = paa_query_procedures;
    plug_in_class->create_procedure = paa_create_procedure;
    plug_in_class->set_i18n         = NULL;
}

static void paa_init(PAA *paa)
{
}

static GimpValueArray* paa_load(GimpProcedure* procedure, GimpRunMode run_mode, GFile* file, GimpMetadata* metadata,
                                GimpMetadataLoadFlags* flags, GimpProcedureConfig* config, gpointer run_data) {

    gchar* filename = g_file_get_path(file);

    auto paa = grad_aff::Paa();
    try {
        paa.readPaa(filename);
    } catch(std::runtime_error& ex) {
        return gimp_procedure_new_return_values(procedure, GIMP_PDB_EXECUTION_ERROR,
                g_error_new(GIMP_PLUG_IN_ERROR, LOAD_PAA_ERROR, (std::string("Exception during Paa Import: \n") + ex.what()).c_str())
        );
    }

    auto width = paa.mipMaps[0].width;
    auto height = paa.mipMaps[0].height;
    auto hasAlpha = paa.hasTransparency;

    GimpImage* image = gimp_image_new(width, height, GIMP_RGB);
    GimpLayer* layer = gimp_layer_new(image, filename,
                                      width, height,
                                      hasAlpha ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE,
                                      100.0, GIMP_LAYER_MODE_NORMAL);

    gboolean success = gimp_image_insert_layer(image, layer, 0, 0);

    if (!success) {
        gimp_image_delete(image);
        return gimp_procedure_new_return_values(procedure, GIMP_PDB_EXECUTION_ERROR,
                g_error_new(GIMP_PLUG_IN_ERROR, LOAD_PAA_ERROR, "Error creating layer")
        );
    }

    auto data = paa.mipMaps[0].data;

    // Remove alpha bytes
    if(!hasAlpha) {
        auto temp = std::vector<uint8_t>();
        temp.reserve((size_t)width * (size_t)height * 3);
        for(size_t i = 0; i < data.size(); i+= 4) {
            temp.push_back(data[i]);
            temp.push_back(data[i + 1]);
            temp.push_back(data[i + 2]);
        }
        data.clear();
        data = temp;
    }

    gegl_init(NULL, NULL);
    GeglBuffer *buffer = gimp_drawable_get_buffer(GIMP_DRAWABLE(layer));

    gegl_buffer_set(buffer, GEGL_RECTANGLE (0, 0, width, height), 0,
                    NULL, data.data(), GEGL_AUTO_ROWSTRIDE);

    g_object_unref (buffer);

    auto return_vals = gimp_procedure_new_return_values(procedure, GIMP_PDB_SUCCESS, NULL);
    GIMP_VALUES_SET_IMAGE (return_vals, 1, image);

    return return_vals;

}

static bool isPowerOfTwo(uint32_t x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

static GimpValueArray* paa_save (GimpProcedure* procedure, GimpRunMode run_mode, GimpImage* image,
                                 GFile* file, GimpExportOptions* options, GimpMetadata* metadata,
                                 GimpProcedureConfig* config, gpointer run_data) {
    gegl_init(NULL, NULL);

    GimpExportReturn exportReturn = gimp_export_options_get_image(options, &image);
    GList *drawables = gimp_image_list_layers(image);
    GimpValueArray* result = savePaa(procedure, g_file_get_path(file), (GimpDrawable*) (drawables->data));

    g_list_free(drawables);
    if (exportReturn == GIMP_EXPORT_EXPORT) {
        gimp_image_delete(image);
    }

    return result;
};

static GimpValueArray* savePaa(GimpProcedure* procedure, const gchar *filename, GimpDrawable* drawable) {

    auto width = gimp_drawable_get_width(drawable);
    auto height = gimp_drawable_get_height(drawable);
    int channelNumber = gimp_drawable_get_bpp(drawable);
    const Babl* format;

    if (channelNumber == 4) {
        format = babl_format("R'G'B'A u8");
    } else {
        format = babl_format("R'G'B' u8");
    }

    if (!isPowerOfTwo(width) || !isPowerOfTwo(height)) {
        return gimp_procedure_new_return_values(procedure, GIMP_PDB_CALLING_ERROR,
            g_error_new(GIMP_PLUG_IN_ERROR, SAVE_PAA_ERROR, "Error during Paa Export:\nDimensions have to be a power of two (2^n)")
        );
    }

    auto data = std::vector<uint8_t>((size_t)width * (size_t)height * channelNumber);
    GeglBuffer* buffer = gimp_drawable_get_buffer(drawable);
    gegl_buffer_get(buffer, GEGL_RECTANGLE(0, 0, width, height),
                    1, format, data.data(), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    g_object_unref (buffer);

    // Insert alpha bytes
    if(channelNumber < 4) {
        auto temp = std::vector<uint8_t>();
        temp.reserve((size_t)width * (size_t)height * 4);
        for(size_t i = 0; i < data.size(); i+= 3) {
            temp.push_back(data[i]);
            temp.push_back(data[i + 1]);
            temp.push_back(data[i + 2]);
            temp.push_back(255);
        }
        data.clear();
        data = temp;
    }

    try {
        auto paa = grad_aff::Paa();
        MipMap mipMap;
        mipMap.height = height;
        mipMap.width = width;
        mipMap.data = data;
        mipMap.dataLength = mipMap.data.size();
        paa.mipMaps.clear();
        paa.mipMaps.push_back(mipMap);
        paa.calculateMipmapsAndTaggs();
        paa.writePaa(filename);
    } catch(std::runtime_error& ex) {
        return gimp_procedure_new_return_values(procedure, GIMP_PDB_EXECUTION_ERROR,
            g_error_new(GIMP_PLUG_IN_ERROR, SAVE_PAA_ERROR, (std::string("Exception during Paa Export: \n") + ex.what()).c_str())
        );
    }
    return gimp_procedure_new_return_values(procedure, GIMP_PDB_SUCCESS, NULL);
}

GIMP_MAIN(paa_get_type())