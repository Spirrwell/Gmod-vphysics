#include "StdAfx.h"

#include "math.h"
#include "Physics_Object.h"
#include "Physics_Environment.h"
#include "Physics_Collision.h"
#include "Physics_FrictionSnapshot.h"
#include "Physics_ShadowController.h"
#include "convert.h"
#include "Physics_DragController.h"
#include "Physics_SurfaceProps.h"

// memdbgon must be the last include file in a .cpp file!!!
//#include "tier0/memdbgon.h"

/***************************
* CLASS CPhysicsObject
***************************/

CPhysicsObject::CPhysicsObject() {
	m_contents = 0;
	m_iGameIndex = 0;
	m_pShadow = NULL;
	m_pFluidController = NULL;
	m_pEnv = NULL;
	m_pGameData = NULL;
	m_pObject = NULL;
	m_pName = NULL;
}

CPhysicsObject::~CPhysicsObject() {
	if (m_pEnv) {
		RemoveShadowController();
		m_pEnv->GetDragController()->RemovePhysicsObject(this);
	}
	
	if (m_pEnv && m_pObject) {
		m_pEnv->GetBulletEnvironment()->removeRigidBody(m_pObject);

		// Sphere collision shape is allocated when we're a sphere. Delete it.
		if (m_bIsSphere)
			delete (btSphereShape *)m_pObject->getCollisionShape();

		delete m_pObject->getMotionState();
		delete m_pObject;
	}
}

bool CPhysicsObject::IsStatic() const {
	return (m_pObject->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT);
}

// Returning true will cause most traces to fail, making the object almost impossible to interact with.
// Also note the lag doesn't stop when the game is paused, indicating that it isn't caused
// by physics simulations.
// Call stack comes from CPhysicsHook:FrameUpdatePostEntityThink to IVEngineServer::SolidMoved
// Perhaps the game is receiving flawed data somewhere, as the engine makes almost no calls to vphysics
bool CPhysicsObject::IsAsleep() const {
	//return m_pObject->getActivationState() == ISLAND_SLEEPING;
	// FIXME: Returning true ensues an extreme lag storm, figure out why since this fix is counter-effective
	return false;
}

bool CPhysicsObject::IsTrigger() const {
	NOT_IMPLEMENTED
	return false;
}

bool CPhysicsObject::IsFluid() const {
	return m_pFluidController != NULL;
}

bool CPhysicsObject::IsHinged() const {
	NOT_IMPLEMENTED
	return false;
}

bool CPhysicsObject::IsMoveable() const {
	if (IsStatic() || !IsMotionEnabled()) return false;
	return true;
}

bool CPhysicsObject::IsAttachedToConstraint(bool bExternalOnly) const {
	// What is bExternalOnly?
	NOT_IMPLEMENTED
	return false;
}

bool CPhysicsObject::IsCollisionEnabled() const {
	return !(m_pObject->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE);
}

bool CPhysicsObject::IsGravityEnabled() const {
	if (!IsStatic()) {
		return !(m_pObject->getFlags() & BT_DISABLE_WORLD_GRAVITY);
	}

	return false;
}

bool CPhysicsObject::IsDragEnabled() const {
	if (!IsStatic()) {
		return m_pEnv->GetDragController()->IsControlling(this);
	}

	return false;
}

bool CPhysicsObject::IsMotionEnabled() const {
	return m_bMotionEnabled;
}

void CPhysicsObject::EnableCollisions(bool enable) {
	if (IsCollisionEnabled() == enable) return;

	if (enable) {
		m_pObject->setCollisionFlags(m_pObject->getCollisionFlags() & ~btCollisionObject::CF_NO_CONTACT_RESPONSE);
	} else {
		m_pObject->setCollisionFlags(m_pObject->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	}
}

void CPhysicsObject::EnableGravity(bool enable) {
	if (IsGravityEnabled() == enable || IsStatic()) return;

	if (enable) {
		m_pObject->setGravity(m_pEnv->GetBulletEnvironment()->getGravity());
		m_pObject->setFlags(m_pObject->getFlags() & ~BT_DISABLE_WORLD_GRAVITY);
	} else {
		m_pObject->setGravity(btVector3(0,0,0));
		m_pObject->setFlags(m_pObject->getFlags() | BT_DISABLE_WORLD_GRAVITY);
	}
}

void CPhysicsObject::EnableDrag(bool enable)  {
	if (IsStatic() || enable == IsDragEnabled())
		return;

	if (enable) {
		m_pEnv->GetDragController()->AddPhysicsObject(this);
	} else {
		m_pEnv->GetDragController()->RemovePhysicsObject(this);
	}
}

void CPhysicsObject::EnableMotion(bool enable) {
	if (IsMotionEnabled() == enable || IsStatic()) return;
	m_bMotionEnabled = enable;

	// TODO: Does this cause any issues with player controllers?
	if (enable) {
		m_pObject->setLinearFactor(btVector3(1, 1, 1));
		m_pObject->setAngularFactor(1);
	} else {
		m_pObject->setLinearVelocity(btVector3(0, 0, 0));
		m_pObject->setAngularVelocity(btVector3(0, 0, 0));

		m_pObject->setLinearFactor(btVector3(0, 0, 0));
		m_pObject->setAngularFactor(0);
	}
}

void CPhysicsObject::SetGameData(void *pGameData) {
	m_pGameData = pGameData;
}

void *CPhysicsObject::GetGameData() const {
	return m_pGameData;
}

void CPhysicsObject::SetGameFlags(unsigned short userFlags) {
	m_gameFlags = userFlags;
}

unsigned short CPhysicsObject::GetGameFlags() const {
	return m_gameFlags;
}

void CPhysicsObject::SetGameIndex(unsigned short gameIndex) {
	m_iGameIndex = gameIndex;
}

unsigned short CPhysicsObject::GetGameIndex() const {
	return m_iGameIndex;
}

void CPhysicsObject::SetCallbackFlags(unsigned short callbackflags) {
	m_callbacks = callbackflags;
}

unsigned short CPhysicsObject::GetCallbackFlags() const {
	return m_callbacks;
}

// UNEXPOSED
void CPhysicsObject::AddCallbackFlags(unsigned short flags) {
	m_callbacks |= flags;
}

// UNEXPOSED
void CPhysicsObject::RemoveCallbackFlags(unsigned short flags) {
	m_callbacks &= ~(flags);
}

void CPhysicsObject::Wake() {
	m_pObject->setActivationState(ACTIVE_TAG);
}

void CPhysicsObject::Sleep() {
	m_pObject->setActivationState(ISLAND_SLEEPING);
}

void CPhysicsObject::RecheckCollisionFilter() {
	// Bullet caches nothing about what should collide with what
}

void CPhysicsObject::RecheckContactPoints() {
	
}

void CPhysicsObject::SetMass(float mass) {
	if (IsStatic()) return;

	btVector3 btvec = m_pObject->getInvInertiaDiagLocal();

	// Invert the inverse intertia to get inertia
	btvec.setX(SAFE_DIVIDE(1.0, btvec.x()));
	btvec.setY(SAFE_DIVIDE(1.0, btvec.y()));
	btvec.setZ(SAFE_DIVIDE(1.0, btvec.z()));

	m_pObject->setMassProps(mass, btvec);
}

float CPhysicsObject::GetMass() const {
	btScalar invmass = m_pObject->getInvMass();
	return SAFE_DIVIDE(1.0, invmass);
}

float CPhysicsObject::GetInvMass() const {
	return m_pObject->getInvMass();
}

Vector CPhysicsObject::GetInertia() const {
	btVector3 btvec = m_pObject->getInvInertiaDiagLocal();

	// Invert the inverse intertia to get inertia
	btvec.setX(SAFE_DIVIDE(1.0, btvec.x()));
	btvec.setY(SAFE_DIVIDE(1.0, btvec.y()));
	btvec.setZ(SAFE_DIVIDE(1.0, btvec.z()));

	Vector hlvec;
	ConvertDirectionToHL(btvec, hlvec);
	VectorAbs(hlvec, hlvec);
	return hlvec;
}

Vector CPhysicsObject::GetInvInertia() const {
	btVector3 btvec = m_pObject->getInvInertiaDiagLocal();
	Vector hlvec;
	ConvertDirectionToHL(btvec, hlvec);
	VectorAbs(hlvec, hlvec);
	return hlvec;
}

void CPhysicsObject::SetInertia(const Vector &inertia) {
	btVector3 btvec;
	ConvertDirectionToBull(inertia, btvec);
	btvec = btvec.absolute();

	btvec.setX(SAFE_DIVIDE(1.0, btvec.x()));
	btvec.setY(SAFE_DIVIDE(1.0, btvec.y()));
	btvec.setZ(SAFE_DIVIDE(1.0, btvec.z()));

	m_pObject->setInvInertiaDiagLocal(btvec);
	m_pObject->updateInertiaTensor();
}

// FIXME: The API is confusing because we need to add the BT_DISABLE_WORLD_GRAVITY flag to the object
// by calling EnableGravity(false)
void CPhysicsObject::SetGravity(const Vector &gravityVector) {
	btVector3 tmp;
	ConvertPosToBull(gravityVector, tmp);
	m_pObject->setGravity(tmp);
}

Vector CPhysicsObject::GetGravity() const {
	Vector tmp;
	ConvertPosToHL(m_pObject->getGravity(), tmp);
	return tmp;
}

void CPhysicsObject::SetDamping(const float *speed, const float *rot) {
	if (!speed && !rot) return;

	if (speed && rot) {
		m_pObject->setDamping(*speed, *rot);
		return;
	}

	if (speed) m_pObject->setDamping(*speed, m_pObject->getAngularDamping());
	if (rot) m_pObject->setDamping(m_pObject->getLinearDamping(), *rot);
}

void CPhysicsObject::GetDamping(float *speed, float *rot) const {
	if (speed) *speed = m_pObject->getLinearDamping();
	if (rot) *rot = m_pObject->getAngularDamping();
}

void CPhysicsObject::SetDragCoefficient(float *pDrag, float *pAngularDrag) {
	if (pDrag)
		m_dragCoefficient = *pDrag;

	if (pAngularDrag)
		m_angDragCoefficient = *pAngularDrag;
}

void CPhysicsObject::SetBuoyancyRatio(float ratio) {
	m_fBuoyancyRatio = ratio;
}

int CPhysicsObject::GetMaterialIndex() const {
	return m_materialIndex;
}

void CPhysicsObject::SetMaterialIndex(int materialIndex) {
	surfacedata_t *pSurface = g_SurfaceDatabase.GetSurfaceData(materialIndex);
	if (pSurface) {
		m_materialIndex = materialIndex;
		m_pObject->setFriction(pSurface->physics.friction);
		//m_pObject->setRollingFriction(pSurface->physics.friction);
		m_pObject->setRestitution(pSurface->physics.elasticity > 1 ? 1 : pSurface->physics.elasticity);

		// FIXME: Figure out how to convert damping values.

		// ratio = (mass / (volume * CUBIC_METERS_PER_CUBIC_INCH)) / density
		m_fBuoyancyRatio = SAFE_DIVIDE(SAFE_DIVIDE(m_fMass, m_fVolume * CUBIC_METERS_PER_CUBIC_INCH), pSurface->physics.density);
	}
}

unsigned int CPhysicsObject::GetContents() const {
	return m_contents;
}

void CPhysicsObject::SetContents(unsigned int contents) {
	m_contents = contents;
}

void CPhysicsObject::SetSleepThresholds(const float *linVel, const float *angVel) {
	if (linVel && angVel) {
		m_pObject->setSleepingThresholds(ConvertDistanceToBull(*linVel), DEG2RAD(*angVel));
	}

	if (linVel) {
		m_pObject->setSleepingThresholds(ConvertDistanceToBull(*linVel), m_pObject->getAngularSleepingThreshold());
	}

	if (angVel) {
		m_pObject->setSleepingThresholds(m_pObject->getLinearSleepingThreshold(), DEG2RAD(*angVel));
	}
}

void CPhysicsObject::GetSleepThresholds(float *linVel, float *angVel) const {
	if (linVel) {
		*linVel = ConvertDistanceToHL(m_pObject->getLinearSleepingThreshold());
	}

	if (angVel) {
		*angVel = RAD2DEG(m_pObject->getAngularSleepingThreshold());
	}
}

float CPhysicsObject::GetSphereRadius() const {
	btCollisionShape *shape = m_pObject->getCollisionShape();
	if (shape->getShapeType() != SPHERE_SHAPE_PROXYTYPE)
		return 0;

	btSphereShape *sphere = (btSphereShape *)shape;
	return ConvertDistanceToHL(sphere->getRadius());
}

float CPhysicsObject::GetEnergy() const {
	float e = 0.5 * GetMass() * m_pObject->getLinearVelocity().dot(m_pObject->getLinearVelocity());
	e += 0.5 * GetMass() * m_pObject->getAngularVelocity().dot(m_pObject->getAngularVelocity());
	return ConvertEnergyToHL(e);
}

// Local space means from the origin of the model
Vector CPhysicsObject::GetMassCenterLocalSpace() const {
	btTransform bullTransform = ((btMassCenterMotionState *)m_pObject->getMotionState())->m_centerOfMassOffset;
	Vector HLMassCenter;
	ConvertPosToHL(bullTransform.getOrigin(), HLMassCenter);

	return HLMassCenter;
}

void CPhysicsObject::SetPosition(const Vector &worldPosition, const QAngle &angles, bool isTeleport) {
	btVector3 bullPos;
	btMatrix3x3 bullAngles;
	ConvertPosToBull(worldPosition, bullPos);
	ConvertRotationToBull(angles, bullAngles);
	m_pObject->setWorldTransform(btTransform(bullAngles, bullPos) * ((btMassCenterMotionState *)m_pObject->getMotionState())->m_centerOfMassOffset);

	if (isTeleport)
		m_pObject->activate();
}

void CPhysicsObject::SetPositionMatrix(const matrix3x4_t &matrix, bool isTeleport) {
	btTransform trans;
	ConvertMatrixToBull(matrix, trans);
	m_pObject->setWorldTransform(trans * ((btMassCenterMotionState *)m_pObject->getMotionState())->m_centerOfMassOffset);

	if (isTeleport)
		m_pObject->activate();
}

void CPhysicsObject::GetPosition(Vector *worldPosition, QAngle *angles) const {
	if (!worldPosition && !angles) return;

	btTransform transform;
	((btMassCenterMotionState *)m_pObject->getMotionState())->getGraphicTransform(transform);
	if (worldPosition) ConvertPosToHL(transform.getOrigin(), *worldPosition);
	if (angles) ConvertRotationToHL(transform.getBasis(), *angles);
}

void CPhysicsObject::GetPositionMatrix(matrix3x4_t *positionMatrix) const {
	if (!positionMatrix) return;

	btTransform transform;
	((btMassCenterMotionState*)m_pObject->getMotionState())->getGraphicTransform(transform);
	ConvertMatrixToHL(transform, *positionMatrix);
}

void CPhysicsObject::SetVelocity(const Vector *velocity, const AngularImpulse *angularVelocity) {
	if (!velocity && !angularVelocity) return;

	btVector3 vel, angvel;
	if (velocity) {
		ConvertPosToBull(*velocity, vel);
		m_pObject->setLinearVelocity(vel);
	}
	if (angularVelocity) {
		ConvertAngularImpulseToBull(*angularVelocity, angvel);
		m_pObject->setAngularVelocity(angvel);
	}
}

void CPhysicsObject::SetVelocityInstantaneous(const Vector *velocity, const AngularImpulse *angularVelocity) {
	// FIXME: what is different from SetVelocity?
	// Sets velocity in the same "iteration"
	SetVelocity(velocity, angularVelocity);
}

void CPhysicsObject::GetVelocity(Vector *velocity, AngularImpulse *angularVelocity) const {
	if (!velocity && !angularVelocity) return;

	if (velocity)
		ConvertPosToHL(m_pObject->getLinearVelocity(), *velocity);

	if (angularVelocity)
		ConvertAngularImpulseToHL(m_pObject->getAngularVelocity(), *angularVelocity);
}

void CPhysicsObject::AddVelocity(const Vector *velocity, const AngularImpulse *angularVelocity) {
	if (!velocity && !angularVelocity) return;

	btVector3 bullvelocity, bullangular;
	if (velocity) {
		ConvertPosToBull(*velocity, bullvelocity);
		m_pObject->setLinearVelocity(m_pObject->getLinearVelocity() + bullvelocity);
	}
	if (angularVelocity) {
		ConvertAngularImpulseToBull(*angularVelocity, bullangular);
		m_pObject->setAngularVelocity(m_pObject->getAngularVelocity() + bullangular);
	}
}

void CPhysicsObject::GetVelocityAtPoint(const Vector &worldPosition, Vector *pVelocity) const {
	if (!pVelocity) return;

	// FIXME: Doesn't getVelocityInLocalPoint take a vector in local space?
	btVector3 vec;
	ConvertPosToBull(worldPosition, vec);
	ConvertPosToHL(m_pObject->getVelocityInLocalPoint(vec), *pVelocity);
}

void CPhysicsObject::GetImplicitVelocity(Vector *velocity, AngularImpulse *angularVelocity) const {
	if (!velocity && !angularVelocity) return;

	NOT_IMPLEMENTED
}

void CPhysicsObject::LocalToWorld(Vector *worldPosition, const Vector &localPosition) const {
	if (!worldPosition) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorTransform(localPosition, matrix, *worldPosition);
}

void CPhysicsObject::WorldToLocal(Vector *localPosition, const Vector &worldPosition) const {
	if (!localPosition) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorITransform(Vector(worldPosition), matrix, *localPosition);
}

void CPhysicsObject::LocalToWorldVector(Vector *worldVector, const Vector &localVector) const {
	if (!worldVector) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorRotate(Vector(localVector), matrix, *worldVector);
}

void CPhysicsObject::WorldToLocalVector(Vector *localVector, const Vector &worldVector) const {
	if (!localVector) return;

	matrix3x4_t matrix;
	GetPositionMatrix(&matrix);
	VectorIRotate(Vector(worldVector), matrix, *localVector);
}

// These two functions apply an insanely low force!
void CPhysicsObject::ApplyForceCenter(const Vector &forceVector) {
	btVector3 force;
	ConvertForceImpulseToBull(forceVector, force);
	m_pObject->applyCentralForce(force);
}

void CPhysicsObject::ApplyForceOffset(const Vector &forceVector, const Vector &worldPosition) {
	Vector local;
	WorldToLocal(&local, worldPosition);

	btVector3 force, offset;
	ConvertForceImpulseToBull(forceVector, force);
	ConvertPosToBull(local, offset);
	m_pObject->applyForce(force, offset);
}

void CPhysicsObject::ApplyTorqueCenter(const AngularImpulse &torque) {
	btVector3 bullTorque;
	ConvertAngularImpulseToBull(torque, bullTorque);
	m_pObject->applyTorque(bullTorque);
}

void CPhysicsObject::CalculateForceOffset(const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque) const {
	if (!centerForce && !centerTorque) return;
	NOT_IMPLEMENTED
}

// Thrusters call this
void CPhysicsObject::CalculateVelocityOffset(const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity) const {
	if (!centerVelocity && !centerAngularVelocity) return;

	btVector3 force, pos;
	ConvertForceImpulseToBull(forceVector, force);
	ConvertPosToBull(worldPosition, pos);

	pos = pos - m_pObject->getWorldTransform().getOrigin();
	btVector3 cross = pos.cross(force);

	NOT_IMPLEMENTED
}

float CPhysicsObject::CalculateLinearDrag(const Vector &unitDirection) const {
	btVector3 bull_unitDirection;
	ConvertDirectionToBull(unitDirection, bull_unitDirection);
	return GetDragInDirection(bull_unitDirection);
}

float CPhysicsObject::CalculateAngularDrag(const Vector &objectSpaceRotationAxis) const {
	btVector3 bull_unitDirection;
	ConvertDirectionToBull(objectSpaceRotationAxis, bull_unitDirection);
	return DEG2RAD(GetAngularDragInDirection(bull_unitDirection));
}

bool CPhysicsObject::GetContactPoint(Vector *contactPoint, IPhysicsObject **contactObject) const {
	if (!contactPoint && !contactObject) return true;

	int numManifolds = m_pEnv->GetBulletEnvironment()->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++) {
		btPersistentManifold *contactManifold = m_pEnv->GetBulletEnvironment()->getDispatcher()->getManifoldByIndexInternal(i);
		const btCollisionObject *obA = contactManifold->getBody0();
		const btCollisionObject *obB = contactManifold->getBody1();

		if (contactManifold->getNumContacts() <= 0)
			continue;

		// Does it matter what the index is, or should we just return the first point of contact?
		btManifoldPoint bullContactPoint = contactManifold->getContactPoint(0);

		if (obA == m_pObject) {
			btVector3 bullContactVec = bullContactPoint.getPositionWorldOnA();

			if (contactPoint) {
				ConvertPosToHL(bullContactVec, *contactPoint);
			}

			if (contactObject) {
				*contactObject = (IPhysicsObject *)obB->getUserPointer();
			}

			return true;
		} else if (obB == m_pObject) {
			btVector3 bullContactVec = bullContactPoint.getPositionWorldOnB();

			if (contactPoint) {
				ConvertPosToHL(bullContactVec, *contactPoint);
			}

			if (contactObject) {
				*contactObject = (IPhysicsObject *)obA->getUserPointer();
			}

			return true;
		}
	}

	return false; // Bool in contact
}

void CPhysicsObject::SetShadow(float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation) {
	if (m_pShadow) {
		m_pShadow->MaxSpeed(maxSpeed, maxAngularSpeed);
		m_pShadow->SetAllowsTranslation(allowPhysicsMovement);
		m_pShadow->SetAllowsRotation(allowPhysicsRotation);
	} else {
		unsigned int flags = GetCallbackFlags() | CALLBACK_SHADOW_COLLISION;
		flags &= ~CALLBACK_GLOBAL_FRICTION;
		flags &= ~CALLBACK_GLOBAL_COLLIDE_STATIC;
		SetCallbackFlags(flags);

		m_pShadow = (CShadowController *)m_pEnv->CreateShadowController(this, allowPhysicsMovement, allowPhysicsRotation);
		m_pShadow->MaxSpeed(maxSpeed, maxAngularSpeed);
	}
}

void CPhysicsObject::UpdateShadow(const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset) {
	if (m_pShadow) {
		m_pShadow->Update(targetPosition, targetAngles, timeOffset);
	}
}

int CPhysicsObject::GetShadowPosition(Vector *position, QAngle *angles) const {
	if (!position && !angles) return 1;

	btTransform transform;
	((btMassCenterMotionState *)m_pObject->getMotionState())->getGraphicTransform(transform);

	if (position)
		ConvertPosToHL(transform.getOrigin(), *position);

	if (angles)
		ConvertRotationToHL(transform.getBasis(), *angles);

	return 0; // return pVEnv->GetSimulatedPSIs();
}

IPhysicsShadowController *CPhysicsObject::GetShadowController() const {
	return m_pShadow;
}

void CPhysicsObject::RemoveShadowController() {
	if (m_pShadow)
		m_pEnv->DestroyShadowController(m_pShadow);
	RemoveCallbackFlags(CALLBACK_SHADOW_COLLISION);
	AddCallbackFlags(CALLBACK_GLOBAL_FRICTION | CALLBACK_GLOBAL_COLLIDE_STATIC);

	m_pShadow = NULL;
}

float CPhysicsObject::ComputeShadowControl(const hlshadowcontrol_params_t &params, float secondsToArrival, float dt) {
	return ComputeShadowControllerHL(this, params, secondsToArrival, dt);
}

const CPhysCollide *CPhysicsObject::GetCollide() const {
	return (CPhysCollide *)m_pObject->getCollisionShape();
}

const char *CPhysicsObject::GetName() const {
	return m_pName;
}

void CPhysicsObject::BecomeTrigger() {
	NOT_IMPLEMENTED
}

void CPhysicsObject::RemoveTrigger() {
	NOT_IMPLEMENTED
}

void CPhysicsObject::BecomeHinged(int localAxis) {
	NOT_IMPLEMENTED
}

void CPhysicsObject::RemoveHinged() {
	NOT_IMPLEMENTED
}

IPhysicsFrictionSnapshot *CPhysicsObject::CreateFrictionSnapshot() {
	return ::CreateFrictionSnapshot(this);
}

void CPhysicsObject::DestroyFrictionSnapshot(IPhysicsFrictionSnapshot *pSnapshot) {
	delete (CPhysicsFrictionSnapshot *)pSnapshot;
}

void CPhysicsObject::OutputDebugInfo() const {
	Msg("-----------------\n");

	if (m_pName)
		Msg("Object: %s\n", m_pName);

	Msg("Mass: %f (inv %f)\n", GetMass(), GetInvMass());

	Vector pos;
	QAngle ang;
	GetPosition(&pos, &ang);
	Msg("Position: %f %f %f\nAngle: %f %f %f\n", pos.x, pos.y, pos.z, ang.x, ang.y, ang.z);

	Vector inertia = GetInertia();
	Vector invinertia = GetInvInertia();
	Msg("Inertia: %f %f %f (inv %f %f %f)\n", inertia.x, inertia.y, inertia.z, invinertia.x, invinertia.y, invinertia.z);

	Vector vel;
	AngularImpulse angvel;
	GetVelocity(&vel, &angvel);
	Msg("Velocity: %f, %f, %f\nAng Velocity: %f, %f, %f\n", vel.x, vel.y, vel.z, angvel.x, angvel.y, angvel.z);

	float dampspeed, damprot;
	GetDamping(&dampspeed, &damprot);
	Msg("Damping %f linear, %f angular\n", dampspeed, damprot);

	Vector dragBasis;
	Vector angDragBasis;
	ConvertPosToHL(m_dragBasis, dragBasis);
	ConvertDirectionToHL(m_angDragBasis, angDragBasis);
	Msg("Linear Drag: %f, %f, %f (factor %f)\n", dragBasis.x, dragBasis.y, dragBasis.z, m_dragCoefficient);
	Msg("Angular Drag: %f, %f, %f (factor %f)\n", angDragBasis.x, angDragBasis.y, angDragBasis.z, m_angDragCoefficient);

	// Attached to x controllers



	Msg("State: %s, Collision %s, Motion %s, Drag %s, Flags %04X (game %04x, index %d)\n", 
		IsAsleep() ? "Asleep" : "Awake",
		IsCollisionEnabled() ? "Enabled" : "Disabled",
		IsStatic() ? "Static" : IsMotionEnabled() ? "Enabled" : "Disabled",
		IsDragEnabled() ? "Enabled" : "Disabled",
		m_pObject->getFlags(),
		GetGameFlags(),
		GetGameIndex()
	);

	
	const char *pMaterialStr = g_SurfaceDatabase.GetPropName(m_materialIndex);
	surfacedata_t *surfaceData = g_SurfaceDatabase.GetSurfaceData(m_materialIndex);
	if (surfaceData) {
		Msg("Material: %s : density(%f), thickness(%f), friction(%f), elasticity(%f)\n", 
			pMaterialStr, surfaceData->physics.density, surfaceData->physics.thickness, surfaceData->physics.friction, surfaceData->physics.elasticity);
	}

	Msg("-- COLLISION SHAPE INFO --\n");
	g_PhysicsCollision.OutputDebugInfo((CPhysCollide *)m_pObject->getCollisionShape());

	// FIXME: complete this function via format noted on
	// http://facepunch.com/threads/1178143?p=35663773&viewfull=1#post35663773
}

// UNEXPOSED
void CPhysicsObject::Init(CPhysicsEnvironment *pEnv, btRigidBody *pObject, int materialIndex, objectparams_t *pParams, bool isStatic, bool isSphere) {
	m_pEnv				= pEnv;
	m_pObject			= pObject;
	m_bIsSphere			= isSphere;
	m_gameFlags			= 0;
	m_bMotionEnabled	= !isStatic;
	m_fMass				= GetMass();
	m_pGameData			= NULL;
	m_pName				= NULL;
	m_fVolume			= 0;
	m_callbacks			= CALLBACK_GLOBAL_COLLISION | CALLBACK_GLOBAL_FRICTION | CALLBACK_FLUID_TOUCH | CALLBACK_GLOBAL_TOUCH | CALLBACK_GLOBAL_COLLIDE_STATIC | CALLBACK_DO_FLUID_SIMULATION;
	m_iLastActivationState = pObject->getActivationState();

	m_pObject->setUserPointer(this);
	m_pObject->setSleepingThresholds(SLEEP_LINEAR_THRESHOLD, SLEEP_ANGULAR_THRESHOLD);

	if (pParams) {
		m_pGameData		= pParams->pGameData;
		m_pName			= pParams->pName;
		m_fVolume		= pParams->volume;
		EnableCollisions(pParams->enableCollisions);
	}

	SetMaterialIndex(materialIndex);
	SetContents(MASK_SOLID);

	// Compute our air drag values.
	float drag = 0;
	float angDrag = 0;
	if (pParams) {
		drag = pParams->dragCoefficient;
		angDrag = pParams->dragCoefficient;
	}

	if (isStatic || !GetCollide()) {
		drag = 0;
		angDrag = 0;
	}

	ComputeDragBasis(isStatic);

	if (!isStatic && drag != 0.0f) {
		EnableDrag(true);
	}

	m_dragCoefficient = drag;
	m_angDragCoefficient = angDrag;

	// Compute our continuous collision detection stuff (for fast moving objects, prevents tunneling)
	if (!isStatic) {
		btVector3 mins, maxs;
		m_pObject->getCollisionShape()->getAabb(btTransform::getIdentity(), mins, maxs);
		mins = mins.absolute();
		maxs = maxs.absolute();

		float maxradius = min(min(maxs.getX(), maxs.getY()), maxs.getZ());
		float minradius = min(min(mins.getX(), mins.getY()), mins.getZ());
		float radius = min(maxradius, minradius);

		m_pObject->setCcdMotionThreshold(radius);
		m_pObject->setCcdSweptSphereRadius(0.2f * radius);
	}

	if (isStatic) {
		pObject->setCollisionFlags(pObject->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
		pEnv->GetBulletEnvironment()->addRigidBody(pObject, COLGROUP_WORLD, ~COLGROUP_WORLD);
	} else {
		pEnv->GetBulletEnvironment()->addRigidBody(pObject);
	}
}

// UNEXPOSED
CPhysicsEnvironment *CPhysicsObject::GetVPhysicsEnvironment() {
	return m_pEnv;
}

// UNEXPOSED
btRigidBody *CPhysicsObject::GetObject() {
	return m_pObject;
}

// UNEXPOSED
float CPhysicsObject::GetDragInDirection(const btVector3 &dir) const {
	btVector3 out;
	btMatrix3x3 mat = m_pObject->getCenterOfMassTransform().getBasis();
	BtMatrix_vimult(mat, dir, out);

	return m_dragCoefficient * fabs(out.getX() * m_dragBasis.getX()) + 
		fabs(out.getY() * m_dragBasis.getY()) +	
		fabs(out.getZ() * m_dragBasis.getZ());
}

// UNEXPOSED
float CPhysicsObject::GetAngularDragInDirection(const btVector3 &dir) const {
	return m_angDragCoefficient * fabs(dir.getX() * m_angDragBasis.getX()) +
		fabs(dir.getY() * m_angDragBasis.getY()) +
		fabs(dir.getZ() * m_angDragBasis.getZ());
}

// UNEXPOSED
void CPhysicsObject::ComputeDragBasis(bool isStatic) {
	m_dragBasis.setZero();
	m_angDragBasis.setZero();

	if (!isStatic && GetCollide()) {
		btCollisionShape *shape = m_pObject->getCollisionShape();

		btVector3 min, max, delta;
		btTransform ident = btTransform::getIdentity();

		shape->getAabb(ident, min, max);

		delta = max - min;
		delta = delta.absolute();

		m_dragBasis.setX(delta.y() * delta.z());
		m_dragBasis.setY(delta.x() * delta.z());
		m_dragBasis.setZ(delta.x() * delta.y());
		m_dragBasis *= GetInvMass();

		btVector3 ang = m_pObject->getInvInertiaDiagLocal();
		delta *= 0.5;

		m_angDragBasis.setX(AngDragIntegral(ang[0], delta.x(), delta.y(), delta.z()) + AngDragIntegral(ang[0], delta.x(), delta.z(), delta.y()));
		m_angDragBasis.setY(AngDragIntegral(ang[1], delta.y(), delta.x(), delta.z()) + AngDragIntegral(ang[1], delta.y(), delta.z(), delta.x()));
		m_angDragBasis.setZ(AngDragIntegral(ang[2], delta.z(), delta.x(), delta.y()) + AngDragIntegral(ang[2], delta.z(), delta.y(), delta.x()));
	}
}

/************************
* CREATION FUNCTIONS
************************/

CPhysicsObject *CreatePhysicsObject(CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic) {
	btCollisionShape *shape = (btCollisionShape *)pCollisionModel;
	
	btVector3 vector;
	btMatrix3x3 matrix;
	ConvertPosToBull(position, vector);
	ConvertRotationToBull(angles, matrix);
	btTransform transform(matrix, vector);

	PhysicsShapeInfo *shapeInfo = (PhysicsShapeInfo *)shape->getUserPointer();
	btTransform masscenter = btTransform::getIdentity();
	if (shapeInfo) masscenter.setOrigin(shapeInfo->massCenter);

	/*
	if (pParams && pParams->massCenterOverride) {
		btVector3 vecMassCenter;
		ConvertPosToBull(*pParams->massCenterOverride, vecMassCenter);
		masscenter.setOrigin(vecMassCenter);
	}
	*/

	float mass = 0;

	if (pParams && !isStatic)
		mass = pParams->mass;

	btVector3 inertia(0, 0, 0);

	if (!isStatic)
		shape->calculateLocalInertia(mass, inertia);

	btMassCenterMotionState *motionstate = new btMassCenterMotionState(masscenter);
	motionstate->setGraphicTransform(transform);
	btRigidBody::btRigidBodyConstructionInfo info(mass, motionstate, shape, inertia);

	if (pParams) {
		info.m_linearDamping = pParams->damping;
		info.m_angularDamping = pParams->rotdamping;

		// FIXME: We should be using inertia values from source. Figure out a proper conversion.
		// Inertia with props is 1 (always?) and 25 with ragdolls (always?)
		//info.m_localInertia = btVector3(pParams->inertia, pParams->inertia, pParams->inertia);
	}

	btRigidBody *body = new btRigidBody(info);

	CPhysicsObject *pObject = new CPhysicsObject;
	pObject->Init(pEnvironment, body, materialIndex, pParams, isStatic);
	
	return pObject;
}

CPhysicsObject *CreatePhysicsSphere(CPhysicsEnvironment *pEnvironment, float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic) {
	if (!pEnvironment) return NULL;

	// Conversion unnecessary as this is an exposed function.
	btSphereShape *shape = (btSphereShape *)g_PhysicsCollision.SphereToConvex(radius);
	
	btVector3 vector;
	btMatrix3x3 matrix;
	ConvertPosToBull(position, vector);
	ConvertRotationToBull(angles, matrix);
	btTransform transform(matrix, vector);

	float mass = 0;
	float volume = 0;

	if (pParams) {
		mass = isStatic ? 0 : pParams->mass;

		volume = pParams->volume;
		if (volume <= 0) {
			pParams->volume = (4 / 3) * M_PI * radius * radius * radius;
		}
	}

	btMassCenterMotionState *motionstate = new btMassCenterMotionState();
	motionstate->setGraphicTransform(transform);
	btRigidBody::btRigidBodyConstructionInfo info(mass, motionstate, shape);

	btRigidBody *body = new btRigidBody(info);

	CPhysicsObject *pObject = new CPhysicsObject;
	pObject->Init(pEnvironment, body, materialIndex, pParams, isStatic, true);

	return pObject;
}