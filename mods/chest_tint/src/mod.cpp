// Chest Tint : teinte en dorure les coffres qui contiennent un objet important.
//
// Principe :
//  1. Un hook sur daTbox_c::Draw note quel coffre est en train d'etre dessine.
//  2. Un hook sur setLightTevColorType_MAJI (la fonction qui applique les couleurs d'eclairage
//     au modele) modifie legerement les couleurs du coffre, mais seulement si son contenu est
//     dans la liste IMPORTANT_ITEMS ci-dessous.
//
// Le contenu lu est celui du coffre APRES le passage du randomizer (le jeu ecrit l'objet
// randomise dans le coffre a sa creation), donc getItemNo() donne deja le bon objet.

#include "global.h"

#include "d/actor/d_a_tbox.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "mods/service.hpp"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"

DEFINE_MOD();
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(LogService, svc_log);

namespace {

// ---------------------------------------------------------------------------------------------
// REGLAGES : modifie ces valeurs pour changer le rendu.
// Les valeurs sont AJOUTEES aux couleurs du coffre (plus c'est haut, plus c'est marque).
// Or / jaune : beaucoup de rouge, un peu moins de vert, pas de bleu.
// ---------------------------------------------------------------------------------------------
constexpr int TINT_AMBIENT_R = 90;
constexpr int TINT_AMBIENT_G = 65;
constexpr int TINT_AMBIENT_B = 0;

constexpr int TINT_TEV_R = 60;
constexpr int TINT_TEV_G = 40;
constexpr int TINT_TEV_B = 0;

// ---------------------------------------------------------------------------------------------
// LISTE DES OBJETS IMPORTANTS : ajoute ou retire des lignes (noms de d_item_data.h).
// ---------------------------------------------------------------------------------------------
bool is_important_item(int item) {
    switch (item) {
    // Objets de progression
    case dItemNo_BOOMERANG_e:
    case dItemNo_SPINNER_e:
    case dItemNo_IRONBALL_e:
    case dItemNo_BOW_e:
    case dItemNo_HOOKSHOT_e:
    case dItemNo_W_HOOKSHOT_e:
    case dItemNo_HVY_BOOTS_e:
    case dItemNo_COPY_ROD_e:
    case dItemNo_COPY_ROD_2_e:
    case dItemNo_KANTERA_e:
    case dItemNo_PACHINKO_e:
    case dItemNo_BOMB_BAG_LV1_e:
    case dItemNo_BOMB_BAG_LV2_e:
    case dItemNo_FISHING_ROD_1_e:
    // Equipement
    case dItemNo_MASTER_SWORD_e:
    case dItemNo_LIGHT_SWORD_e:
    case dItemNo_HYLIA_SHIELD_e:
    case dItemNo_ARMOR_e:
    case dItemNo_WEAR_ZORA_e:
    // Cles speciales
    case dItemNo_BOSS_KEY_e:
    case dItemNo_LV2_BOSS_KEY_e:
    case dItemNo_LV5_BOSS_KEY_e:
    case dItemNo_BOSSRIDER_KEY_e:
    case dItemNo_L2_KEY_PIECES1_e:
    case dItemNo_L2_KEY_PIECES2_e:
    case dItemNo_L2_KEY_PIECES3_e:
    case dItemNo_MIRROR_PIECE_2_e:
    case dItemNo_MIRROR_PIECE_3_e:
    case dItemNo_MIRROR_PIECE_4_e:
    // Coeurs
    case dItemNo_KAKERA_HEART_e:
    case dItemNo_UTAWA_HEART_e:
        return true;
    default:
        return false;
    }
}

// Coffre en cours de dessin (nullptr si le coffre n'est pas "important").
daTbox_c* g_drawingChest = nullptr;

int clamp_int(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

DEFINE_HOOK(&daTbox_c::Draw, TboxDraw);
DEFINE_HOOK(static_cast<void (dScnKy_env_light_c::*)(J3DModelData*, dKy_tevstr_c*)>(
                &dScnKy_env_light_c::setLightTevColorType_MAJI),
    LightTevColorMaji);

HookAction on_tbox_draw_pre(ModContext*, void* args, void*, void*) {
    daTbox_c* chest = mods::arg<daTbox_c*>(args, 0);
    g_drawingChest = (chest != nullptr && is_important_item(chest->getItemNo())) ? chest : nullptr;
    return HOOK_CONTINUE;
}

void on_tbox_draw_post(ModContext*, void*, void*, void*) {
    g_drawingChest = nullptr;
}

HookAction on_light_color_pre(ModContext*, void* args, void*, void*) {
    if (g_drawingChest == nullptr || g_drawingChest->mpModel == nullptr) {
        return HOOK_CONTINUE;
    }

    // args[0] = this (env light), args[1] = donnees du modele, args[2] = couleurs du coffre.
    J3DModelData* modelData = mods::arg<J3DModelData*>(args, 1);
    dKy_tevstr_c* tev = mods::arg<dKy_tevstr_c*>(args, 2);

    // Ne touche qu'au modele du coffre lui-meme (pas a l'effet lumineux qui partage les couleurs).
    if (tev != &g_drawingChest->tevStr || modelData != g_drawingChest->mpModel->getModelData()) {
        return HOOK_CONTINUE;
    }

    tev->AmbCol.r = static_cast<s16>(clamp_int(tev->AmbCol.r + TINT_AMBIENT_R, 0, 255));
    tev->AmbCol.g = static_cast<s16>(clamp_int(tev->AmbCol.g + TINT_AMBIENT_G, 0, 255));
    tev->AmbCol.b = static_cast<s16>(clamp_int(tev->AmbCol.b + TINT_AMBIENT_B, 0, 255));

    tev->TevColor.r = static_cast<s16>(clamp_int(tev->TevColor.r + TINT_TEV_R, -1024, 1023));
    tev->TevColor.g = static_cast<s16>(clamp_int(tev->TevColor.g + TINT_TEV_G, -1024, 1023));
    tev->TevColor.b = static_cast<s16>(clamp_int(tev->TevColor.b + TINT_TEV_B, -1024, 1023));

    return HOOK_CONTINUE;
}

}  // namespace

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    if (mods::hook::add_pre<TboxDraw>(on_tbox_draw_pre) != MOD_OK ||
        mods::hook::add_post<TboxDraw>(on_tbox_draw_post) != MOD_OK)
    {
        return mods::set_error(error, MOD_ERROR, "chest_tint: impossible de hooker daTbox_c::Draw");
    }
    if (mods::hook::add_pre<LightTevColorMaji>(on_light_color_pre) != MOD_OK) {
        return mods::set_error(
            error, MOD_ERROR, "chest_tint: impossible de hooker setLightTevColorType_MAJI");
    }
    svc_log->info(mod_ctx, "chest_tint initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    g_drawingChest = nullptr;
    svc_log->info(mod_ctx, "chest_tint unloaded");
    return MOD_OK;
}
}
