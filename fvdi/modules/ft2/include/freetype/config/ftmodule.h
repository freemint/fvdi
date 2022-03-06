#include "ft2version.h"

#if FREETYPE_VERSION >= 2003008L
/* since 2.3.8, this macro includes the variable type */
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, t1_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, t1cid_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, pfr_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, t42_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, winfnt_driver_class )
/* FT_USE_MODULE( FT_Driver_ClassRec, gemfnt_driver_class ) */
FT_USE_MODULE( FT_Driver_ClassRec, pcf_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, bdf_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
#if FREETYPE_VERSION <= 2010002L
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_lcd_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_lcdv_renderer_class )
#endif
#else
FT_USE_MODULE(autofit_module_class)
FT_USE_MODULE(tt_driver_class)
FT_USE_MODULE(t1_driver_class)
FT_USE_MODULE(cff_driver_class)
FT_USE_MODULE(t1cid_driver_class)
FT_USE_MODULE(pfr_driver_class)
FT_USE_MODULE(t42_driver_class)
FT_USE_MODULE(winfnt_driver_class)
FT_USE_MODULE(pcf_driver_class) 
FT_USE_MODULE(bdf_driver_class)
FT_USE_MODULE(psaux_module_class)
FT_USE_MODULE(psnames_module_class)
FT_USE_MODULE(pshinter_module_class)
FT_USE_MODULE(sfnt_module_class)
FT_USE_MODULE(ft_raster1_renderer_class)
FT_USE_MODULE(ft_smooth_renderer_class)
#endif
