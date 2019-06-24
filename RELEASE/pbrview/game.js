
// Create light
var light = new THREE.PointLight(0xffffff, 0.5, 100, 2);
light.position.set(0, 0, 6);
//scene.add(light);

// Create and load textures
var base = texture("basecolor.png");
var normal = texture("normal.png");
var metal = texture("metallic.png");
var rough = texture("roughness.png");
var occlusion = texture("occlusion.png");

// Create and load the env map
var loader = new THREE.CubeTextureLoader();
loader.setPath( 'envs/Park2/' );
var ext = 'jpg';
var textureCube  = loader.load( [
	'px.'+ext, 'nx.'+ext,
	'py.'+ext, 'ny.'+ext,
	'pz.'+ext, 'nz.'+ext
] );

// Cube is a geometry, mesh and material:
var material = new THREE.MeshStandardMaterial({
	color: 0xffffff,
	envMap: textureCube,
	map: base,
	normalMap: normal,
	normalScale: new THREE.Vector2(1,-1),
	roughnessMap: rough,
	metalnessMap: metal,
	//metalness: 1,
	//roughness: 0,
	aoMap: occlusion,
	aoMapIntensity: 1,

});
var geometry = new THREE.BoxGeometry(1, 1, 1);
var cube = new THREE.Mesh(geometry, material);
//cube.visible = false;
scene.add(cube);


// dir light
var dir = new THREE.DirectionalLight(0xffffff, .8);
dir.position.x = 5;
dir.position.y = 4;
dir.position.z = 5;
dir.target = cube;
scene.add(dir);
var dir2 = new THREE.DirectionalLight(0xffffff, .8);
dir2.position.x = -5;
dir2.position.y = 4;
dir2.position.z = 5;
dir2.target = cube;
scene.add(dir2);

var dir3 = new THREE.DirectionalLight(0xffffff, .3);
dir3.position.x = 0;
dir3.position.y = -1;
dir3.position.z = 0;
dir3.target = cube;
scene.add(dir3);

// Move camera out
camera.position.z = 2.5;

// Update loop 
var sensitivity = 0.01;
var maxf = 55;

function update() // each frame
{
	if(accum.y<-maxf) accum.y=-maxf;
	if(accum.y>maxf) accum.y=maxf;
	cube.rotation.x = accum.y*sensitivity;
	cube.rotation.y = accum.x*sensitivity;
}

var accum = {x:0,y:0}

document.addEventListener('mousemove', function(event) {
	event.preventDefault();
	if(event.buttons == 1)
	{
		accum.x += event.movementX;
		accum.y += event.movementY;
	}
}, false);
document.addEventListener( 'mousewheel', function(event) {
	event.preventDefault();
	camera.position.z -= event.wheelDeltaY*0.002;
}, false);



// API
function setCubeVisible(v)
{
	cube.visible = v;
}