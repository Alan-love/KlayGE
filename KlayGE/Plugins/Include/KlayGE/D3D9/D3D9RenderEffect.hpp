// D3D9RenderEffect.hpp
// KlayGE D3D9渲染效果类 头文件
// Ver 2.5.0
// 版权所有(C) 龚敏敏, 2003-2005
// Homepage: http://klayge.sourceforge.net
//
// 2.5.0
// 去掉了Clone (2005.4.16)
//
// 2.4.0
// 改为派生自D3D9Resource (2005.3.3)
//
// 2.3.0
// 增加了OnLostDevice和OnResetDevice (2005.2.23)
//
// 2.0.4
// 增加了D3D9RenderTechnique (2004.3.16)
//
// 2.0.3
// 修改了SetTexture的参数 (2004.3.6)
// 增加了SetMatrixArray/GetMatrixArray (2004.3.11)
//
// 2.0.0
// 初次建立 (2003.8.15)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#ifndef _D3D9RENDEREFFECT_HPP
#define _D3D9RENDEREFFECT_HPP

#include <KlayGE/PreDeclare.hpp>
#include <boost/smart_ptr.hpp>

#include <map>

#include <d3dx9effect.h>

#include <KlayGE/RenderEffect.hpp>
#include <KlayGE/D3D9/D3D9Resource.hpp>

#ifdef KLAYGE_DEBUG
	#pragma comment(lib, "KlayGE_RenderEngine_D3D9_d.lib")
#else
	#pragma comment(lib, "KlayGE_RenderEngine_D3D9.lib")
#endif

namespace KlayGE
{
	class D3D9RenderEffect;
	class D3D9RenderEffectParameter;

	typedef boost::shared_ptr<D3D9RenderEffect> D3D9RenderEffectPtr;
	typedef boost::shared_ptr<D3D9RenderEffectParameter> D3D9RenderEffectParameterPtr;

	// 渲染效果
	//////////////////////////////////////////////////////////////////////////////////
	class D3D9RenderEffect : public RenderEffect, public D3D9Resource
	{
	public:
		explicit D3D9RenderEffect(std::string const & srcData);

		boost::shared_ptr<ID3DXEffect> const & D3DXEffect() const
			{ return effect_; }

		void Desc(uint32_t& parameters, uint32_t& techniques, uint32_t& functions);

		RenderEffectParameterPtr Parameter(uint32_t index);
		RenderEffectParameterPtr ParameterByName(std::string const & name);
		RenderEffectParameterPtr ParameterBySemantic(std::string const & semantic);

		bool SetTechnique(std::string const & technique);
		bool SetTechnique(uint32_t technique);

		uint32_t Begin(uint32_t flags = 0);
		void BeginPass(uint32_t passNum);
		void EndPass();
		void End();

	private:
		bool Validate(D3DXHANDLE handle);

		void DoOnLostDevice();
		void DoOnResetDevice();

		RenderEffectParameterPtr DoParameter(D3DXHANDLE handle);
		bool DoSetTechnique(D3DXHANDLE handle);

	private:
		boost::shared_ptr<ID3DXEffect> effect_;

		typedef std::map<D3DXHANDLE, D3D9RenderEffectParameterPtr> params_t;
		params_t params_;
	};

	class D3D9RenderEffectParameter : public RenderEffectParameter, public D3D9Resource
	{
	public:
		D3D9RenderEffectParameter(boost::shared_ptr<ID3DXEffect> const & effect, D3DXHANDLE parameter);

		RenderEffectParameter& operator=(float value);
		RenderEffectParameter& operator=(Vector4 const & value);
		RenderEffectParameter& operator=(Matrix4 const & value);
		RenderEffectParameter& operator=(int value);
		RenderEffectParameter& operator=(TexturePtr const & tex);

		operator float() const;
		operator Vector4() const;
		operator Matrix4() const;
		operator int() const;

		void SetFloatArray(float const * value, size_t count);
		void GetFloatArray(float* value, size_t count);
		void SetVectorArray(Vector4 const * value, size_t count);
		void GetVectorArray(Vector4* value, size_t count);
		void SetMatrixArray(Matrix4 const * matrices, size_t count);
		void GetMatrixArray(Matrix4* matrices, size_t count);
		void SetIntArray(int const * value, size_t count);
		void GetIntArray(int* value, size_t count);

	private:
		void DoOnLostDevice();
		void DoOnResetDevice();

	private:
		boost::shared_ptr<ID3DXEffect> effect_;
		D3DXHANDLE parameter_;

		TexturePtr texture_;
	};
}

#endif		// _D3D9RENDEREFFECT_HPP
