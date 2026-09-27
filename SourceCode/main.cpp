#include "Raytracer.h"
#include "Pathtracer.h"
#include "Scene/Animation.h"
#include "Scene/SceneFactory.h"
#include <chrono>
#include <cstdlib>

using namespace std::chrono;

Image testAccelerated(Raytracer& renderer) {
	high_resolution_clock::time_point start = high_resolution_clock::now();
	Image result = renderer.renderAccelerated();
	high_resolution_clock::time_point stop = high_resolution_clock::now();
	microseconds duration = duration_cast<microseconds>(stop - start);
	double seconds = duration.count() / 1'000'000.0;
	std::cout << "Rendering time: " << seconds << std::endl;
	return result;
}

void GI_test(const char* scenePath, const char* outPath, int raysPerPixel) {
    Scene* scene = SceneFactory::factory(scenePath);
	if (raysPerPixel > 0) scene->setRaysPerPixel(raysPerPixel);
	Pathtracer pathtracer(scene);
	pathtracer.renderScene(outPath);
	delete scene;
}

void GI_animation(void) {
	Scene* scene = SceneFactory::factory("Scenes/scene2.scene");
	Animation vertigoAnimation = Animation::vertigoAnimation(scene, 4, 6.62, 30);
	Raytracer raytracer(&vertigoAnimation);

	raytracer.renderAnimation("Images/Project/Animation/GI_vertigo");
	delete scene;
}

void dragonAnimation(void) {
	Scene* scene = SceneFactory::factory("Scenes/scene1.scene");
	Vector sceneMiddle = (scene->getAABB().max + scene->getAABB().min) / 2;
	Animation orbitAnimation = Animation::orbitAnimation(scene, sceneMiddle, 72, 5, false);
	Raytracer raytracer(&orbitAnimation);

	raytracer.renderAnimation("Images/Project/Animation/dragon_orbit");
	delete scene;
}

int main(int argc, char** argv)
{
	const char* scenePath = (argc > 1) ? argv[1] : "Scenes/glass_dragon.scene";
	const char* outPath   = (argc > 2) ? argv[2] : "Images/Project/pt_scene2.ppm";
	int raysPerPixel      = (argc > 3) ? std::atoi(argv[3]) : 0;  // 0 = use scene/Settings default
	GI_test(scenePath, outPath, raysPerPixel);
	// GI_animation();
	// dragonAnimation();
}
