//create a renderer
var renderer = new THREE.WebGLRenderer({antialias:true, alpha:true});
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);
renderer.setClearColor(0x000000, 0);

// Make scene and camera
var scene = new THREE.Scene();
var camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1000);


// APIs
var cachebuster = 0;
function texture(filename)
{
	filename +=  "?" + (++cachebuster);
	var tex = new THREE.TextureLoader();
	tex.setCrossOrigin(null);
	tex = tex.load(filename);
	tex.wrapS = THREE.RepeatWrapping;
	tex.wrapT = THREE.RepeatWrapping;
	tex.anisotropy = 16;
	return tex;
}

function SetTexture(map, file)
{
	var tex = new THREE.TextureLoader();
	tex.load(file +  "?" + (++cachebuster),
		function(T)
		{
			T.anisotropy = 16;
			material[map] = T;
		}
	);
}

function update(){};

// RenderLoop
function animate() {
	requestAnimationFrame(animate);
	update();
	renderer.render(scene, camera);
} animate();

// Resize Event
window.addEventListener( 'resize', onWindowResize, false );
function onWindowResize()
{
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize( window.innerWidth, window.innerHeight );
}