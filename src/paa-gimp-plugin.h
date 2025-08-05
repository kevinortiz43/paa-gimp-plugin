// paa-gimp-plugin.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <grad_aff/paa/paa.h>

#include <filesystem>
#include <vector>
#include <iostream>

#define PLUGIN_NAME "paa-gimp-plugin"
#define LOAD_PROC "file-paa-load"
#define SAVE_PROC "file-paa-save"
#define LOAD_PAA_ERROR -1
#define LOAD_PAA_CANCEL -2
#define SAVE_PAA_ERROR -3

namespace fs = std::filesystem;

struct _PAA
{
    GimpPlugIn parent_instance;
};

G_DECLARE_FINAL_TYPE (PAA, paa, PAA,, GimpPlugIn)

static GList*          paa_query_procedures (GimpPlugIn* plug_in);
static GimpProcedure*  paa_create_procedure (GimpPlugIn* plug_in,
                                             const gchar* name);
static GimpValueArray* paa_load             (GimpProcedure* procedure,
                                             GimpRunMode run_mode,
                                             GFile* file,
                                             GimpMetadata* metadata,
                                             GimpMetadataLoadFlags* flags,
                                             GimpProcedureConfig* config,
                                             gpointer run_data);
static GimpValueArray* paa_save             (GimpProcedure* procedure,
                                             GimpRunMode run_mode,
                                             GimpImage* image,
                                             GFile* file,
                                             GimpExportOptions* options,
                                             GimpMetadata* metadata,
                                             GimpProcedureConfig* config,
                                             gpointer run_data);
static GimpValueArray* savePaa              (GimpProcedure* procedure,
                                             const gchar *filename,
                                             GimpDrawable* drawable);