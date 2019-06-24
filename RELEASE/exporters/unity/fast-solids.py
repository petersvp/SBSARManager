import os
import sys
import time
import json
import shutil
from hashlib import md5

# File Loader
def fdata(s):
    file = open(s,mode='r')
    all_of_it = file.read()
    file.close()
    return all_of_it
    
# File writer
def fsave(s, data):
    file = open(s,mode='w')
    file.write(data)
    file.close()

# Load templates from path[0]
templateMaterial = fdata(sys.path[0] + '/template-material.yaml')
templateMaterialMeta = fdata(sys.path[0] + '/template-meta-material.yaml')
templateTextureMeta = fdata(sys.path[0] + '/template-meta-texture.yaml')

# UUIDs for Unity generated using MD5 hashing
# All uuids from SBSAR Manager will be 5b57-prefixed
# ALWAYS USE SOURCE CONTROL! Colisions MAY occur even if unlikely!
# ALWAYS check if some other meta file gets changed and REVERT IF SO!
# If an uuid is clashing with existing uuid in your project, rename the preset!
def uuid(s):
    md5val = md5(s.encode("utf-8"))
    return "5b57" + md5val.hexdigest()[4:]

# generate the materials from the logfile
with open(sys.argv[1]) as logfile:  
    presets = json.load(logfile)
    
    # each exported preset we will create a material file
    for p in presets:

        cat = p['category']
        preset = p['preset']
        matFileName = cat+'/'+preset+'.mat'
        materialExists = os.path.exists(matFileName)
        
        # create textures folder
        if not os.path.exists(cat+'/textures'): os.mkdir(cat+'/textures')
        
        # generate the material .mat and .meta
        materialId = uuid(cat+'/'+preset)
        materialMeta = templateMaterialMeta.replace("%UUID%", materialId)
        
        # Move Textures inside
        shutil.move(p['maps']['RGBM'], cat+'/textures/'+preset+'-RGBM.png')
        shutil.move(p['maps']['NRMS'], cat+'/textures/'+preset+'-NRMS.png')
               
        # We have NRMS and RGBM maps, fill them in the .mat
        if not materialExists: 
            material = templateMaterial \
            .replace('%RGBMUUID%',uuid(p['maps']['RGBM'])) \
            .replace('%NRMSUUID%',uuid(p['maps']['NRMS']))
            
        # Generate metas for textures
        metaRGBM = templateTextureMeta.replace('%UUID%', uuid(p['maps']['RGBM']))
        metaNRMS = templateTextureMeta.replace('%UUID%', uuid(p['maps']['NRMS']))

        # Save all texture metas
        fsave(cat+'/textures/'+preset+'-RGBM.png.meta', metaRGBM)
        fsave(cat+'/textures/'+preset+'-NRMS.png.meta', metaNRMS)

        # Save Material meta and the material
        if not materialExists: fsave(matFileName, material)
        fsave(matFileName+'.meta', materialMeta)
        
        # Move Textures and their metas inside
        
 