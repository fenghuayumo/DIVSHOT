//#include "precompile.h"
//#include "entity_factory.h"
////#include "Physics/DiversePhysicsEngine/CollisionShapes/SphereCollisionShape.h"
////#include "Physics/DiversePhysicsEngine/CollisionShapes/PyramidCollisionShape.h"
////#include "Physics/DiversePhysicsEngine/CollisionShapes/CuboidCollisionShape.h"
////#include "Physics/DiversePhysicsEngine/DiversePhysicsEngine.h"
////#include "scene/component/model_component.h"
////#include "maths/random.h"
////#include "scene/scene.h"
////#include "core/application.h"
////#include "scene/component/components.h"
////#include "maths/transform.h"
////#include "scene/entity_manager.h"
//
//namespace diverse
//{
//    using namespace maths;
//
//    glm::vec4 EntityFactory::GenColour(float alpha)
//    {
//        glm::vec4 c;
//        c.w = alpha;
//
//        c.x = Random32::Rand(0.0f, 1.0f);
//        c.y = Random32::Rand(0.0f, 1.0f);
//        c.z = Random32::Rand(0.0f, 1.0f);
//
//        return c;
//    }
//
//    Entity EntityFactory::BuildSphereObject(
//        Scene* scene,
//        const std::string& name,
//        const glm::vec3& pos,
//        float radius,
//        bool physics_enabled,
//        float inverse_mass,
//        bool collidable,
//        const glm::vec4& colour)
//    {
//        auto sphere = scene->get_entity_manager()->Create(name);
//        sphere.add_component<Maths::Transform>(glm::translate(glm::mat4(1.0), pos) * glm::scale(glm::mat4(1.0), glm::vec3(radius * 2.0f)));
//        auto& model = sphere.add_component<Graphics::ModelComponent>(Graphics::PrimitiveType::Sphere).ModelRef;
//
//        SharedPtr<Graphics::Material> matInstance = CreateSharedPtr<Graphics::Material>();
//        Graphics::MaterialProperties properties;
//        properties.albedoColour       = colour;
//        properties.roughness          = Random32::Rand(0.0f, 1.0f);
//        properties.metallic           = Random32::Rand(0.0f, 1.0f);
//        properties.albedoMapFactor    = 0.0f;
//        properties.roughnessMapFactor = 0.0f;
//        properties.normalMapFactor    = 0.0f;
//        properties.metallicMapFactor  = 0.0f;
//        matInstance->SetMaterialProperites(properties);
//
//        // auto shader = Application::Get().GetShaderLibrary()->GetResource("//CoreShaders/ForwardPBR.shader");
//        // matInstance->SetShader(nullptr);//shader);
//        model->GetMeshes().front()->SetMaterial(matInstance);
//
//        if(physics_enabled)
//        {
//            // Otherwise create a physics object, and set it's position etc
//            // SharedPtr<RigidBody3D> testPhysics = CreateSharedPtr<RigidBody3D>();
//            RigidBody3D* testPhysics = Application::Get().GetSystem<LumosPhysicsEngine>()->CreateBody({});
//
//            testPhysics->SetPosition(pos);
//            testPhysics->SetInverseMass(inverse_mass);
//
//            if(!collidable)
//            {
//                // Even without a collision shape, the inertia matrix for rotation has to be derived from the objects shape
//                testPhysics->SetInverseInertia(SphereCollisionShape(radius).BuildInverseInertia(inverse_mass));
//            }
//            else
//            {
//                testPhysics->SetCollisionShape(CreateSharedPtr<SphereCollisionShape>(radius));
//                testPhysics->SetInverseInertia(testPhysics->GetCollisionShape()->BuildInverseInertia(inverse_mass));
//            }
//
//            sphere.add_component<RigidBody3DComponent>(testPhysics);
//        }
//        else
//        {
//            sphere.get_transform().set_local_position(pos);
//        }
//
//        return sphere;
//    }
//
//    Entity EntityFactory::BuildCuboidObject(
//        Scene* scene,
//        const std::string& name,
//        const glm::vec3& pos,
//        const glm::vec3& halfdims,
//        bool physics_enabled,
//        float inverse_mass,
//        bool collidable,
//        const glm::vec4& colour)
//    {
//        auto cube = scene->get_entity_manager()->Create(name);
//        cube.add_component<Maths::Transform>(glm::translate(glm::mat4(1.0), pos) * glm::scale(glm::mat4(1.0), halfdims * 2.0f));
//        auto& model = cube.add_component<Graphics::ModelComponent>(Graphics::PrimitiveType::Cube).ModelRef;
//
//        auto matInstance = CreateSharedPtr<Graphics::Material>();
//        Graphics::MaterialProperties properties;
//        properties.albedoColour       = colour;
//        properties.roughness          = Random32::Rand(0.0f, 1.0f);
//        properties.metallic           = Random32::Rand(0.0f, 1.0f);
//        properties.emissive           = 3.0f;
//        properties.albedoMapFactor    = 0.0f;
//        properties.roughnessMapFactor = 0.0f;
//        properties.normalMapFactor    = 0.0f;
//        properties.metallicMapFactor  = 0.0f;
//        properties.emissiveMapFactor  = 0.0f;
//        properties.occlusionMapFactor = 0.0f;
//        matInstance->SetMaterialProperites(properties);
//
//        // auto shader = Application::Get().GetShaderLibrary()->GetResource("//CoreShaders/ForwardPBR.shader");
//        // matInstance->SetShader(shader);
//        model->GetMeshes().front()->SetMaterial(matInstance);
//
//        if(physics_enabled)
//        {
//            // Otherwise create a physics object, and set it's position etc
//            // SharedPtr<RigidBody3D> testPhysics = CreateSharedPtr<RigidBody3D>();
//            RigidBody3D* testPhysics = Application::Get().GetSystem<LumosPhysicsEngine>()->CreateBody({});
//            testPhysics->SetPosition(pos);
//            testPhysics->SetInverseMass(inverse_mass);
//
//            if(!collidable)
//            {
//                // Even without a collision shape, the inertia matrix for rotation has to be derived from the objects shape
//                testPhysics->SetInverseInertia(CuboidCollisionShape(halfdims).BuildInverseInertia(inverse_mass));
//            }
//            else
//            {
//                testPhysics->SetCollisionShape(CreateSharedPtr<CuboidCollisionShape>(halfdims));
//                testPhysics->SetInverseInertia(testPhysics->GetCollisionShape()->BuildInverseInertia(inverse_mass));
//            }
//
//            cube.add_component<RigidBody3DComponent>(testPhysics);
//        }
//        else
//        {
//            cube.get_transform().set_local_position(pos);
//        }
//
//        return cube;
//    }
//
//    Entity EntityFactory::BuildPyramidObject(
//        Scene* scene,
//        const std::string& name,
//        const glm::vec3& pos,
//        const glm::vec3& halfdims,
//        bool physics_enabled,
//        float inverse_mass,
//        bool collidable,
//        const glm::vec4& colour)
//    {
//        auto pyramid           = scene->get_entity_manager()->Create(name);
//        auto pyramidMeshEntity = scene->get_entity_manager()->Create();
//
//        SharedPtr<Graphics::Material> matInstance = CreateSharedPtr<Graphics::Material>();
//        Graphics::MaterialProperties properties;
//        properties.albedoColour       = colour;
//        properties.roughness          = Random32::Rand(0.0f, 1.0f);
//        properties.metallic           = Random32::Rand(0.0f, 1.0f);
//        properties.albedoMapFactor    = 0.0f;
//        properties.roughnessMapFactor = 0.0f;
//        properties.normalMapFactor    = 0.0f;
//        properties.metallicMapFactor  = 0.0f;
//        matInstance->SetMaterialProperites(properties);
//
//        // auto shader = Application::Get().GetShaderLibrary()->GetResource("//CoreShaders/ForwardPBR.shader");
//        // matInstance->SetShader(shader);
//
//        pyramidMeshEntity.add_component<Maths::Transform>(glm::toMat4(glm::quat(glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f))) * glm::scale(glm::mat4(1.0), halfdims));
//        pyramidMeshEntity.set_parent(pyramid);
//        auto& model = pyramidMeshEntity.add_component<Graphics::ModelComponent>(Graphics::PrimitiveType::Pyramid).ModelRef;
//        model->GetMeshes().front()->SetMaterial(matInstance);
//
//        if(physics_enabled)
//        {
//            // Otherwise create a physics object, and set it's position etc
//            // SharedPtr<RigidBody3D> testPhysics = CreateSharedPtr<RigidBody3D>();
//            RigidBody3D* testPhysics = Application::Get().GetSystem<LumosPhysicsEngine>()->CreateBody({});
//            testPhysics->SetPosition(pos);
//            testPhysics->SetInverseMass(inverse_mass);
//
//            if(!collidable)
//            {
//                // Even without a collision shape, the inertia matrix for rotation has to be derived from the objects shape
//                testPhysics->SetInverseInertia(PyramidCollisionShape(halfdims).BuildInverseInertia(inverse_mass));
//            }
//            else
//            {
//                testPhysics->SetCollisionShape(CreateSharedPtr<PyramidCollisionShape>(halfdims));
//                testPhysics->SetInverseInertia(testPhysics->GetCollisionShape()->BuildInverseInertia(inverse_mass));
//            }
//
//            pyramid.add_component<RigidBody3DComponent>(testPhysics);
//            pyramid.get_or_add_component<Maths::Transform>().set_local_position(pos);
//        }
//        else
//        {
//            pyramid.get_transform().set_local_position(pos);
//        }
//
//        return pyramid;
//    }
//
//    void EntityFactory::AddLightCube(Scene* scene, const glm::vec3& pos, const glm::vec3& dir)
//    {
//        glm::vec4 colour = glm::vec4(Random32::Rand(0.0f, 1.0f),
//                                     Random32::Rand(0.0f, 1.0f),
//                                     Random32::Rand(0.0f, 1.0f),
//                                     1.0f);
//
//        entt::registry& registry = scene->get_registry();
//
//        auto cube = EntityFactory::BuildCuboidObject(
//            scene,
//            "light Cube",
//            pos,
//            glm::vec3(0.5f, 0.5f, 0.5f),
//            true,
//            1.0f,
//            true,
//            colour);
//
//        // cube.get_component<RigidBody3DComponent>().GetRigidBody()->SetIsAtRest(true);
//        const float radius    = Random32::Rand(1.0f, 30.0f);
//        const float intensity = Random32::Rand(0.0f, 2.0f) * 120000.0f;
//
//        cube.add_component<Graphics::Light>(pos, colour, intensity, Graphics::LightType::PointLight, pos, radius);
//        const glm::vec3 forward = dir;
//        cube.get_component<RigidBody3DComponent>().GetRigidBody()->SetLinearVelocity(forward * 30.0f);
//    }
//
//    void EntityFactory::AddSphere(Scene* scene, const glm::vec3& pos, const glm::vec3& dir)
//    {
//        entt::registry& registry = scene->get_registry();
//
//        auto sphere = EntityFactory::BuildSphereObject(
//            scene,
//            "Sphere",
//            pos,
//            Random32::Rand(0.8f, 1.7f),
//            true,
//            Random32::Rand(0.2f, 1.0f),
//            true,
//            glm::vec4(Random32::Rand(0.0f, 1.0f),
//                      Random32::Rand(0.0f, 1.0f),
//                      Random32::Rand(0.0f, 1.0f),
//                      1.0f));
//
//        const glm::vec3 forward = dir;
//        sphere.get_component<RigidBody3DComponent>().GetRigidBody()->SetLinearVelocity(forward * 20.0f);
//    }
//
//    void EntityFactory::AddPyramid(Scene* scene, const glm::vec3& pos, const glm::vec3& dir)
//    {
//        entt::registry& registry = scene->get_registry();
//
//        auto sphere = EntityFactory::BuildPyramidObject(
//            scene,
//            "Pyramid",
//            pos,
//            glm::vec3(0.5f),
//            true,
//            1.0f,
//            true,
//            glm::vec4(Random32::Rand(0.0f, 1.0f),
//                      Random32::Rand(0.0f, 1.0f),
//                      Random32::Rand(0.0f, 1.0f),
//                      1.0f));
//
//        const glm::vec3 forward = dir;
//
//        sphere.get_component<RigidBody3DComponent>().GetRigidBody()->SetLinearVelocity(forward * 30.0f);
//    }
//}
