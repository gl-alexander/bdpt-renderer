#include "Raytracer.h"
#include "Pathtracer.h"
#include "Scene/Animation.h"
#include "Scene/SceneFactory.h"
#include <chrono>

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

void GI_test(void) {
    Scene* scene = SceneFactory::factory("Scenes/scene2.scene");
	Pathtracer pathtracer(scene);

	ImageSaver::saveImage("Images/Project/pt_scene2.ppm", testAccelerated(pathtracer));
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

int main()
{
	GI_test();
    GI_animation();
    dragonAnimation();
}
