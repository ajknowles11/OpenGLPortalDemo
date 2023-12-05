bl_info = {
    "name": "Portal Data",
    "author": "Andrew Knowles",
    "version": (0, 4),
    "blender": (2, 80, 0),
    "location": "Properties > Object > Portal Data",
    "description": "Allows meshes to be marked as portals with destinations, and adds some object types.",
    "warning": "",
    "doc_url": "",
    "category": "Genious",
}


import bpy

from bpy.props import (BoolProperty,
                       PointerProperty,
                       EnumProperty,
                       StringProperty,
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
    
    walk_mesh: PointerProperty(
        name = "Associated WalkMesh",
        description = "The WalkMesh on which this portal sits, i.e. where the player is placed when exiting this portal",
        type = bpy.types.Object
    )

    is_button: BoolProperty(
        name = "Is Button",
        description = "Whether this object should be exported as a button, with crosshair check",
        default = False
    )

    is_trigger: BoolProperty(
        name = "Is Trigger",
        description = "Whether this object should be exported as a trigger, calling a function when entered",
        default = False
    )

    def program_callback(self, context):
        return (
            ('OUTLINED', 'Outlined', 'Normal outlined with post processing'),
            ('LITCOLORTEX', 'LitColorTexture', 'Use LitColorTextureProgram (no outline)')
        )

    shader_program: EnumProperty(  
        items = program_callback,
        name = "Shader Program",
        description = "Shader program and amount of post-processing for this mesh",
        default = None,
    )

    portal_group: StringProperty(
        name = "Portal Group",
        description = "Group this portal is assigned to, and will be enabled/disabled with",
        default = ""
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
        layout.prop(portaldata, "walk_mesh")
        layout.prop(portaldata, "is_button")
        layout.prop(portaldata, "is_trigger")
        layout.prop(portaldata, "shader_program")
        layout.prop(portaldata, "portal_group")

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
