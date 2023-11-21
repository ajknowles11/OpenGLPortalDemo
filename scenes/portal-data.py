bl_info = {
    "name": "Portal Data",
    "author": "Andrew Knowles",
    "version": (0, 1),
    "blender": (2, 80, 0),
    "location": "Properties > Object > Portal Data",
    "description": "Allows meshes to be marked as portals with destinations",
    "warning": "",
    "doc_url": "",
    "category": "Genious",
}


import bpy

from bpy.props import (BoolProperty,
                       PointerProperty,
                       )

from bpy.types import (Panel,
                       PropertyGroup,
                       Object,
                       )


# Properties

class PortalProperties(PropertyGroup):
    
    is_portal: BoolProperty(
        name = "Is Portal",
        description = "Whether this object should be exported as a portal",
        default = False
        )
    
    dest: PointerProperty(
        name = "Destination Portal",
        description = "Destination for this portal- where the player sees from and is teleported to",
        type = bpy.types.Object
        )

# Panel

class UV_PT_portals_panel(Panel):
    bl_idname = "UV_PT_portals_panel"
    bl_label = "Portal Data"
    bl_category = "Genious"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    
    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        portaldata = obj.portal_data

        layout.prop(portaldata, "is_portal")
        layout.prop(portaldata, "dest")

# Registration

classes = (
    PortalProperties,
    UV_PT_portals_panel,
    )


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
    
    bpy.types.Object.portal_data = PointerProperty(type=PortalProperties)


def unregister():
    from bpy.utils import unregister_class
    for cls in reversed(classes):
        unregister_class(cls)
    
    del bpy.types.Object.portal_data


if __name__ == "__main__":
    register()
