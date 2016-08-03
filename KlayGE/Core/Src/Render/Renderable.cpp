// Renderable.cpp
// KlayGE 可渲染对象类 实现文件
// Ver 3.4.0
// 版权所有(C) 龚敏敏, 2003-2006
// Homepage: http://www.klayge.org
//
// 3.4.0
// 修正了Render没有设置Technique的bug (2006.7.26)
//
// 2.7.0
// GetWorld改名为GetModelMatrix (2005.6.17)
//
// 2.3.0
// 增加了Render (2005.1.15)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KFL/Math.hpp>
#include <KlayGE/SceneManager.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/SceneObject.hpp>
#include <KlayGE/RenderEffect.hpp>
#include <KlayGE/FrameBuffer.hpp>
#include <KlayGE/Camera.hpp>
#include <KlayGE/RenderMaterial.hpp>
#include <KlayGE/DeferredRenderingLayer.hpp>

#include <KlayGE/Renderable.hpp>

namespace KlayGE
{
	Renderable::Renderable()
		: select_mode_on_(false),
			model_mat_(float4x4::Identity()), effect_attrs_(0)
	{
		auto drl = Context::Instance().DeferredRenderingLayerInstance();
		if (drl)
		{
			this->BindDeferredEffect(drl->GBufferEffect());
		}
	}

	Renderable::~Renderable()
	{
	}

	void Renderable::OnRenderBegin()
	{
		RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();
		Camera const & camera = *re.CurFrameBuffer()->GetViewport()->camera;
		float4x4 const & view = camera.ViewMatrix();
		float4x4 const & proj = camera.ProjMatrix();
		float4x4 mv = model_mat_ * view;
		float4x4 mvp = mv * proj;
		AABBox const & pos_bb = this->PosBound();
		AABBox const & tc_bb = this->TexcoordBound();

		auto drl = Context::Instance().DeferredRenderingLayerInstance();

		if (drl)
		{
			int32_t cas_index = drl->CurrCascadeIndex();
			if (cas_index >= 0)
			{
				mvp *= drl->GetCascadedShadowLayer()->CascadeCropMatrix(cas_index);
			}
		}

		if (select_mode_on_)
		{
			*mvp_param_ = mvp;
			*pos_center_param_ = pos_bb.Center();
			*pos_extent_param_ = pos_bb.HalfSize();
			*select_mode_object_id_param_ = select_mode_object_id_;
		}
		else if (drl)
		{
			*mvp_param_ = mvp;
			*model_view_param_ = mv;
			*far_plane_param_ = float2(camera.FarPlane(), 1.0f / camera.FarPlane());
			*forward_vec_param_ = camera.ForwardVec();

			FrameBufferPtr const & fb = re.CurFrameBuffer();
			*frame_size_param_ = int2(fb->Width(), fb->Height());

			*height_offset_scale_param_ = mtl_ ? mtl_->height_offset_scale : float2(-0.5f, 0.06f);
			*tess_factors_param_ = mtl_ ? mtl_->tess_factors : float4(5, 5, 1, 9);

			*pos_center_param_ = pos_bb.Center();
			*pos_extent_param_ = pos_bb.HalfSize();
			*tc_center_param_ = float2(tc_bb.Center().x(), tc_bb.Center().y());
			*tc_extent_param_ = float2(tc_bb.HalfSize().x(), tc_bb.HalfSize().y());

			switch (type_)
			{
			case PT_OpaqueDepth:
			case PT_TransparencyBackDepth:
			case PT_TransparencyFrontDepth:
				*albedo_tex_param_ = albedo_tex_;
				*albedo_clr_param_ = mtl_ ? mtl_->albedo : float4(0, 0, 0, 1);
				*albedo_map_enabled_param_ = static_cast<int32_t>(!!albedo_tex_);
				*alpha_test_threshold_param_ = mtl_ ? mtl_->alpha_test : 0;
				*opaque_depth_tex_param_ = drl->CurrFrameDepthTex(drl->ActiveViewport());
				break;

			case PT_OpaqueGBufferRT0:
			case PT_TransparencyBackGBufferRT0:
			case PT_TransparencyFrontGBufferRT0:
			case PT_OpaqueGBufferRT1:
			case PT_TransparencyBackGBufferRT1:
			case PT_TransparencyFrontGBufferRT1:
			case PT_OpaqueGBufferMRT:
			case PT_TransparencyBackGBufferMRT:
			case PT_TransparencyFrontGBufferMRT:
			case PT_GenReflectiveShadowMap:
				*albedo_tex_param_ = albedo_tex_;
				*albedo_clr_param_ = mtl_ ? mtl_->albedo : float4(0, 0, 0, 1);
				*albedo_map_enabled_param_ = static_cast<int32_t>(!!albedo_tex_);
				*metalness_clr_param_ = float2(mtl_ ? mtl_->metalness : 0, static_cast<float>(!!metalness_tex_));
				*metalness_tex_param_ = metalness_tex_;
				*alpha_test_threshold_param_ = mtl_ ? mtl_->alpha_test : 0;
				*normal_map_enabled_param_ = static_cast<int32_t>(!!normal_tex_);
				*normal_tex_param_ = normal_tex_;
				if (!mtl_ || (RenderMaterial::SDM_Parallax == mtl_->detail_mode))
				{
					*height_map_parallax_enabled_param_ = static_cast<int32_t>(!!height_tex_);
				}
				else
				{
					*height_map_tess_enabled_param_ = static_cast<int32_t>(!!height_tex_);
				}
				*height_tex_param_ = height_tex_;
				*glossiness_clr_param_ = float2(MathLib::clamp(mtl_ ? mtl_->glossiness : 0, 1e-6f, 0.999f),
					static_cast<float>(!!glossiness_tex_));
				*glossiness_tex_param_ = glossiness_tex_;
				*opaque_depth_tex_param_ = drl->CurrFrameDepthTex(drl->ActiveViewport());
				break;

			case PT_GenShadowMap:
			case PT_GenCascadedShadowMap:
			case PT_GenShadowMapWODepthTexture:
				*albedo_tex_param_ = albedo_tex_;
				*albedo_clr_param_ = mtl_ ? mtl_->albedo : float4(0, 0, 0, 1);
				*albedo_map_enabled_param_ = static_cast<int32_t>(!!albedo_tex_);
				*alpha_test_threshold_param_ = mtl_ ? mtl_->alpha_test : 0;
				break;

			case PT_OpaqueShading:
			case PT_TransparencyBackShading:
			case PT_TransparencyFrontShading:
				*glossiness_clr_param_ = float2(MathLib::clamp(mtl_ ? mtl_->glossiness : 0, 1e-6f, 0.999f),
					static_cast<float>(!!glossiness_tex_));
				*glossiness_tex_param_ = glossiness_tex_;
				*albedo_tex_param_ = albedo_tex_;
				*albedo_clr_param_ = mtl_ ? mtl_->albedo : float4(0, 0, 0, 1);
				*albedo_map_enabled_param_ = static_cast<int32_t>(!!albedo_tex_);
				*metalness_clr_param_ = float2(mtl_ ? mtl_->metalness : 0, static_cast<float>(!!metalness_tex_));
				*metalness_tex_param_ = metalness_tex_;
				*alpha_test_threshold_param_ = mtl_ ? mtl_->alpha_test : 0;
				*emissive_tex_param_ = emissive_tex_;
				*emissive_clr_param_ = float4(
					mtl_ ? mtl_->emissive.x() : 0, mtl_ ? mtl_->emissive.y() : 0, mtl_ ? mtl_->emissive.z() : 0,
					static_cast<float>(!!emissive_tex_));
				break;

			case PT_OpaqueReflection:
			case PT_TransparencyBackReflection:
			case PT_TransparencyFrontReflection:
				*albedo_tex_param_ = albedo_tex_;
				*albedo_clr_param_ = mtl_ ? mtl_->albedo : float4(0, 0, 0, 1);
				*albedo_map_enabled_param_ = static_cast<int32_t>(!!albedo_tex_);
				break;

			case PT_OpaqueSpecialShading:
			case PT_TransparencyBackSpecialShading:
			case PT_TransparencyFrontSpecialShading:
				*albedo_tex_param_ = albedo_tex_;
				*albedo_clr_param_ = mtl_ ? mtl_->albedo : float4(0, 0, 0, 1);
				*albedo_map_enabled_param_ = static_cast<int32_t>(!!albedo_tex_);
				*alpha_test_threshold_param_ = mtl_ ? mtl_->alpha_test : 0;
				*emissive_tex_param_ = emissive_tex_;
				*emissive_clr_param_ = float4(
					mtl_ ? mtl_->emissive.x() : 0, mtl_ ? mtl_->emissive.y() : 0, mtl_ ? mtl_->emissive.z() : 0,
					static_cast<float>(!!emissive_tex_));
				if (reflection_tex_param_)
				{
					*reflection_tex_param_ = drl->ReflectionTex(drl->ActiveViewport());
				}
				break;

			default:
				break;
			}
		}
	}

	void Renderable::OnRenderEnd()
	{
	}

	void Renderable::OnInstanceBegin(uint32_t /*id*/)
	{
	}

	void Renderable::OnInstanceEnd(uint32_t /*id*/)
	{
	}

	void Renderable::AddToRenderQueue()
	{
		Context::Instance().SceneManagerInstance().AddRenderable(this);
	}

	void Renderable::Render()
	{
		this->UpdateInstanceStream();

		RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();

		RenderLayout const & layout = this->GetRenderLayout();
		GraphicsBufferPtr const & inst_stream = layout.InstanceStream();
		RenderTechnique const & tech = *this->GetRenderTechnique();
		auto const & effect = *this->GetRenderEffect();
		if (inst_stream)
		{
			if (layout.NumInstances() > 0)
			{
				this->OnRenderBegin();
				re.Render(effect, tech, layout);
				this->OnRenderEnd();
			}
		}
		else
		{
			this->OnRenderBegin();
			if (instances_.empty())
			{
				re.Render(effect, tech, layout);
			}
			else
			{
				for (uint32_t i = 0; i < instances_.size(); ++ i)
				{
					this->OnInstanceBegin(i);
					re.Render(effect, tech, layout);
					this->OnInstanceEnd(i);
				}
			}
			this->OnRenderEnd();
		}
	}

	void Renderable::AddInstance(SceneObject const * obj)
	{
		instances_.push_back(obj);
	}

	void Renderable::UpdateInstanceStream()
	{
		if (!instances_.empty() && !instances_[0]->InstanceFormat().empty())
		{
			vertex_elements_type const & vet = instances_[0]->InstanceFormat();
			uint32_t size = 0;
			for (size_t i = 0; i < vet.size(); ++ i)
			{
				size += vet[i].element_size();
			}

			uint32_t const inst_size = static_cast<uint32_t>(size * instances_.size());

			RenderLayout& rl = this->GetRenderLayout();

			GraphicsBufferPtr inst_stream = rl.InstanceStream();
			if (inst_stream && (inst_stream->Size() >= inst_size))
			{
				for (size_t i = 0; i < instances_.size(); ++ i)
				{
					BOOST_ASSERT(rl.InstanceStreamFormat() == instances_[i]->InstanceFormat());
				}
			}
			else
			{
				RenderFactory& rf(Context::Instance().RenderFactoryInstance());
				inst_stream = rf.MakeVertexBuffer(BU_Dynamic, EAH_CPU_Write | EAH_GPU_Read, inst_size, nullptr);
				rl.BindVertexStream(inst_stream, vet, RenderLayout::ST_Instance, 1);
				rl.InstanceStream(inst_stream);
			}

			{
				GraphicsBuffer::Mapper mapper(*inst_stream, BA_Write_Only);
				for (size_t i = 0; i < instances_.size(); ++ i)
				{
					uint8_t const * src = static_cast<uint8_t const *>(instances_[i]->InstanceData());
					std::copy(src, src + size, mapper.Pointer<uint8_t>() + i * size);
				}
			}

			for (uint32_t i = 0; i < rl.NumVertexStreams(); ++ i)
			{
				rl.VertexStreamFrequencyDivider(i, RenderLayout::ST_Geometry, static_cast<uint32_t>(instances_.size()));
			}
		}
	}

	void Renderable::ModelMatrix(float4x4 const & mat)
	{
		model_mat_ = mat;
	}

	void Renderable::UpdateBoundBox()
	{
	}

	bool Renderable::AllHWResourceReady() const
	{
		bool ready = this->HWResourceReady();
		if (ready && albedo_tex_)
		{
			ready = albedo_tex_->HWResourceReady();
		}
		if (ready && metalness_tex_)
		{
			ready = metalness_tex_->HWResourceReady();
		}
		if (ready && glossiness_tex_)
		{
			ready = glossiness_tex_->HWResourceReady();
		}
		if (ready && emissive_tex_)
		{
			ready = emissive_tex_->HWResourceReady();
		}
		if (ready && normal_tex_)
		{
			ready = normal_tex_->HWResourceReady();
		}
		if (ready && height_tex_)
		{
			ready = height_tex_->HWResourceReady();
		}
		return ready;
	}

	void Renderable::ObjectID(uint32_t id)
	{
		select_mode_object_id_ = float4(((id & 0xFF) + 0.5f) / 255.0f,
			(((id >> 8) & 0xFF) + 0.5f) / 255.0f, (((id >> 16) & 0xFF) + 0.5f) / 255.0f, 0.0f);
	}

	void Renderable::SelectMode(bool select_mode)
	{
		select_mode_on_ = select_mode;
		if (select_mode_on_)
		{
			technique_ = select_mode_tech_;
		}
	}

	void Renderable::Pass(PassType type)
	{
		type_ = type;
		technique_ = this->PassTech(type);
	}

	void Renderable::BindDeferredEffect(RenderEffectPtr const & deferred_effect)
	{
		deferred_effect_ = deferred_effect;
		effect_ = deferred_effect;

		this->UpdateTechniques();

		mvp_param_ = deferred_effect_->ParameterByName("mvp");
		model_view_param_ = deferred_effect_->ParameterByName("model_view");
		far_plane_param_ = deferred_effect_->ParameterByName("far_plane");
		forward_vec_param_ = deferred_effect_->ParameterByName("forward_vec");
		frame_size_param_ = deferred_effect_->ParameterByName("frame_size");
		height_offset_scale_param_ = deferred_effect_->ParameterByName("height_offset_scale");
		tess_factors_param_ = deferred_effect_->ParameterByName("tess_factors");
		pos_center_param_ = deferred_effect_->ParameterByName("pos_center");
		pos_extent_param_ = deferred_effect_->ParameterByName("pos_extent");
		tc_center_param_ = deferred_effect_->ParameterByName("tc_center");
		tc_extent_param_ = deferred_effect_->ParameterByName("tc_extent");
		albedo_map_enabled_param_ = deferred_effect_->ParameterByName("albedo_map_enabled");
		albedo_tex_param_ = deferred_effect_->ParameterByName("albedo_tex");
		albedo_clr_param_ = deferred_effect_->ParameterByName("albedo_clr");
		metalness_clr_param_ = deferred_effect_->ParameterByName("metalness_clr");
		metalness_tex_param_ = deferred_effect_->ParameterByName("metalness_tex");
		glossiness_clr_param_ = deferred_effect_->ParameterByName("glossiness_clr");
		glossiness_tex_param_ = deferred_effect_->ParameterByName("glossiness_tex");
		emissive_tex_param_ = deferred_effect_->ParameterByName("emissive_tex");
		emissive_clr_param_ = deferred_effect_->ParameterByName("emissive_clr");
		normal_map_enabled_param_ = deferred_effect_->ParameterByName("normal_map_enabled");
		normal_tex_param_ = deferred_effect_->ParameterByName("normal_tex");
		height_map_parallax_enabled_param_ = deferred_effect_->ParameterByName("height_map_parallax_enabled");
		height_map_tess_enabled_param_ = deferred_effect_->ParameterByName("height_map_tess_enabled");
		height_tex_param_ = deferred_effect_->ParameterByName("height_tex");
		opaque_depth_tex_param_ = deferred_effect_->ParameterByName("opaque_depth_tex");
		reflection_tex_param_ = nullptr;
		alpha_test_threshold_param_ = deferred_effect_->ParameterByName("alpha_test_threshold");
		select_mode_object_id_param_ = deferred_effect_->ParameterByName("object_id");
	}

	void Renderable::UpdateTechniques()
	{
		bool sss;
		RenderMaterial::SurfaceDetailMode sdm;
		if (mtl_)
		{
			sss = mtl_->sss;
			sdm = mtl_->detail_mode;
		}
		else
		{
			sss = false;
			sdm = RenderMaterial::SDM_Parallax;
		}

		switch (sdm)
		{
		case RenderMaterial::SDM_Parallax:
			if (this->AlphaTest())
			{
				if (sss)
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("SSSGBufferAlphaTestMRTTech");
				}
				else
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaTestMRTTech");
				}
			}
			else
			{
				if (sss)
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("SSSGBufferMRTTech");
				}
				else
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferMRTTech");
				}
			}
			gbuffer_alpha_blend_back_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaBlendBackMRTTech");
			gbuffer_alpha_blend_front_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaBlendFrontMRTTech");
			special_shading_tech_ = deferred_effect_->TechniqueByName("SpecialShadingTech");
			special_shading_alpha_blend_back_tech_ = deferred_effect_->TechniqueByName("SpecialShadingAlphaBlendBackTech");
			special_shading_alpha_blend_front_tech_ = deferred_effect_->TechniqueByName("SpecialShadingAlphaBlendFrontTech");
			break;
		
		case RenderMaterial::SDM_FlatTessellation:
			if (this->AlphaTest())
			{
				if (sss)
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("SSSGBufferFlatTessAlphaTestMRTTech");
				}
				else
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferFlatTessAlphaTestMRTTech");
				}
			}
			else
			{
				if (sss)
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("SSSGBufferFlatTessMRTTech");
				}
				else
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferFlatTessMRTTech");
				}
			}
			gbuffer_alpha_blend_back_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferFlatTessAlphaBlendBackMRTTech");
			gbuffer_alpha_blend_front_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferFlatTessAlphaBlendFrontMRTTech");
			special_shading_tech_ = deferred_effect_->TechniqueByName("SpecialShadingFlatTessTech");
			special_shading_alpha_blend_back_tech_ = deferred_effect_->TechniqueByName("SpecialShadingFlatTessAlphaBlendBackTech");
			special_shading_alpha_blend_front_tech_ = deferred_effect_->TechniqueByName("SpecialShadingFlatTessAlphaBlendFrontTech");
			break;

		case RenderMaterial::SDM_SmoothTessellation:
			if (this->AlphaTest())
			{
				if (sss)
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("SSSGBufferSmoothTessAlphaTestMRTTech");
				}
				else
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferSmoothTessAlphaTestMRTTech");
				}
			}
			else
			{
				if (sss)
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("SSSGBufferSmoothTessMRTTech");
				}
				else
				{
					gbuffer_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferSmoothTessMRTTech");
				}
			}
			gbuffer_alpha_blend_back_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferSmoothTessAlphaBlendBackMRTTech");
			gbuffer_alpha_blend_front_mrt_tech_ = deferred_effect_->TechniqueByName("GBufferSmoothTessAlphaBlendFrontMRTTech");
			special_shading_tech_ = deferred_effect_->TechniqueByName("SpecialShadingSmoothTessTech");
			special_shading_alpha_blend_back_tech_ = deferred_effect_->TechniqueByName("SpecialShadingSmoothTessAlphaBlendBackTech");
			special_shading_alpha_blend_front_tech_ = deferred_effect_->TechniqueByName("SpecialShadingSmoothTessAlphaBlendFrontTech");
			break;

		default:
			BOOST_ASSERT(false);
			break;
		}

		if (this->AlphaTest())
		{
			if (sss)
			{
				depth_tech_ = deferred_effect_->TechniqueByName("SSSDepthAlphaTestTech");
				gbuffer_rt0_tech_ = deferred_effect_->TechniqueByName("SSSGBufferAlphaTestRT0Tech");
				gbuffer_rt1_tech_ = deferred_effect_->TechniqueByName("SSSGBufferAlphaTestRT1Tech");
				gen_sm_tech_ = deferred_effect_->TechniqueByName("SSSGenShadowMapAlphaTestTech");
				gen_sm_wo_dt_tech_ = deferred_effect_->TechniqueByName("SSSGenShadowMapWODepthTextureAlphaTestTech");
			}
			else
			{
				depth_tech_ = deferred_effect_->TechniqueByName("DepthAlphaTestTech");
				gbuffer_rt0_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaTestRT0Tech");
				gbuffer_rt1_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaTestRT1Tech");
				gen_rsm_tech_ = deferred_effect_->TechniqueByName("GenReflectiveShadowMapAlphaTestTech");
				gen_sm_tech_ = deferred_effect_->TechniqueByName("GenShadowMapAlphaTestTech");
				gen_cascaded_sm_tech_ = deferred_effect_->TechniqueByName("GenCascadedShadowMapAlphaTestTech");
				gen_sm_wo_dt_tech_ = deferred_effect_->TechniqueByName("GenShadowMapWODepthTextureAlphaTestTech");
			}
		}
		else
		{
			if (sss)
			{
				depth_tech_ = deferred_effect_->TechniqueByName("SSSDepthTech");
				gbuffer_rt0_tech_ = deferred_effect_->TechniqueByName("SSSGBufferRT0Tech");
				gbuffer_rt1_tech_ = deferred_effect_->TechniqueByName("SSSGBufferRT1Tech");
				gen_sm_tech_ = deferred_effect_->TechniqueByName("SSSGenShadowMapTech");
				gen_sm_wo_dt_tech_ = deferred_effect_->TechniqueByName("SSSGenShadowMapWODepthTextureTech");
			}
			else
			{
				depth_tech_ = deferred_effect_->TechniqueByName("DepthTech");
				gbuffer_rt0_tech_ = deferred_effect_->TechniqueByName("GBufferRT0Tech");
				gbuffer_rt1_tech_ = deferred_effect_->TechniqueByName("GBufferRT1Tech");
				gen_rsm_tech_ = deferred_effect_->TechniqueByName("GenReflectiveShadowMapTech");
				gen_sm_tech_ = deferred_effect_->TechniqueByName("GenShadowMapTech");
				gen_cascaded_sm_tech_ = deferred_effect_->TechniqueByName("GenCascadedShadowMapTech");
				gen_sm_wo_dt_tech_ = deferred_effect_->TechniqueByName("GenShadowMapWODepthTextureTech");
			}
		}
		depth_alpha_blend_back_tech_ = deferred_effect_->TechniqueByName("DepthAlphaBlendBackTech");
		depth_alpha_blend_front_tech_ = deferred_effect_->TechniqueByName("DepthAlphaBlendFrontTech");
		gbuffer_alpha_blend_back_rt0_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaBlendBackRT0Tech");
		gbuffer_alpha_blend_front_rt0_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaBlendFrontRT0Tech");
		gbuffer_alpha_blend_back_rt1_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaBlendBackRT1Tech");
		gbuffer_alpha_blend_front_rt1_tech_ = deferred_effect_->TechniqueByName("GBufferAlphaBlendFrontRT1Tech");
		select_mode_tech_ = deferred_effect_->TechniqueByName("SelectModeTech");
	}

	RenderTechnique* Renderable::PassTech(PassType type) const
	{
		switch (type)
		{
		case PT_OpaqueDepth:
			return depth_tech_;

		case PT_TransparencyBackDepth:
			return depth_alpha_blend_back_tech_;

		case PT_TransparencyFrontDepth:
			return depth_alpha_blend_front_tech_;

		case PT_OpaqueGBufferRT0:
			return gbuffer_rt0_tech_;

		case PT_OpaqueGBufferRT1:
			return gbuffer_rt1_tech_;

		case PT_TransparencyBackGBufferRT0:
			return gbuffer_alpha_blend_back_rt0_tech_;

		case PT_TransparencyBackGBufferRT1:
			return gbuffer_alpha_blend_back_rt1_tech_;

		case PT_TransparencyFrontGBufferRT0:
			return gbuffer_alpha_blend_front_rt0_tech_;

		case PT_TransparencyFrontGBufferRT1:
			return gbuffer_alpha_blend_front_rt1_tech_;

		case PT_OpaqueGBufferMRT:
			return gbuffer_mrt_tech_;

		case PT_TransparencyBackGBufferMRT:
			return gbuffer_alpha_blend_back_mrt_tech_;

		case PT_TransparencyFrontGBufferMRT:
			return gbuffer_alpha_blend_front_mrt_tech_;

		case PT_GenReflectiveShadowMap:
			return gen_rsm_tech_;

		case PT_GenShadowMap:
			return gen_sm_tech_;

		case PT_GenCascadedShadowMap:
			return gen_cascaded_sm_tech_;

		case PT_GenShadowMapWODepthTexture:
			return gen_sm_wo_dt_tech_;

		case PT_OpaqueReflection:
			return reflection_tech_;

		case PT_TransparencyBackReflection:
			return reflection_alpha_blend_back_tech_;

		case PT_TransparencyFrontReflection:
			return reflection_alpha_blend_front_tech_;

		case PT_OpaqueSpecialShading:
			return special_shading_tech_;

		case PT_TransparencyBackSpecialShading:
			return special_shading_alpha_blend_back_tech_;

		case PT_TransparencyFrontSpecialShading:
			return special_shading_alpha_blend_front_tech_;

		case PT_SimpleForward:
			return simple_forward_tech_;

		default:
			BOOST_ASSERT(false);
			return gbuffer_rt0_tech_;
		}
	}
}
