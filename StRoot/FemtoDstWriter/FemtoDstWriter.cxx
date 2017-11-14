
#include "FemtoDstFormat/BranchWriter.h"
#include "FemtoDstWriter.h"

#include "StThreeVectorF.hh"

#include "StEvent.h"
#include "StTrack.h"
#include "StGlobalTrack.h"
#include "StTrackGeometry.h"
#include "StDcaGeometry.h"
#include "StMtdPidTraits.h"
#include "StBTofPidTraits.h"
#include "StPhysicalHelixD.hh"
#include "StThreeVectorD.hh"
#include "StEvent/StTrackNode.h"
#include "StEvent/StGlobalTrack.h"
#include "StEvent/StRunInfo.h"
#include "StEvent/StEventInfo.h"
#include "StEvent/StPrimaryVertex.h"
#include "StEvent/StBTofHeader.h"


#include "StMuDSTMaker/COMMON/StMuDstMaker.h"
#include "StMuDSTMaker/COMMON/StMuDst.h"
#include "StMuDSTMaker/COMMON/StMuEvent.h"
#include "StMuDSTMaker/COMMON/StMuTrack.h"
#include "StMuDSTMaker/COMMON/StMuPrimaryVertex.h"
#include "StMuDSTMaker/COMMON/StMuMtdHit.h"
#include "StMuDSTMaker/COMMON/StMuMtdPidTraits.h"



#include "StarClassLibrary/StParticleDefinition.hh"

// StPicoDstMaker
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoDstMaker/StPicoDst.h"
#include "StPicoDstMaker/StPicoEvent.h"
#include "StPicoDstMaker/StPicoTrack.h"
#include "StPicoDstMaker/StPicoMtdPidTraits.h"
#include "StPicoDstMaker/StPicoMtdHit.h"

// STL
#include <vector>
#include <map>
#include <algorithm>

ClassImp(FemtoDstWriter)


FemtoDstWriter::FemtoDstWriter( const Char_t *name ) : StMaker( name )
{
	this->_outputFilename = "FemtoDst.root";

	SetDebug(1);
}

FemtoDstWriter::~FemtoDstWriter()
{

}

Int_t FemtoDstWriter::Init()
{

	LOG_INFO << "INITIALIZE" <<endm;
	this->_rootFile = new TFile( this->_outputFilename.c_str(), "RECREATE" );
	this->_rootFile->cd();
	this->_tree = new TTree( "FemtoDst", "FemtoDst with MC info" );

	// Create Event Branch
	_few.createBranch( this->_tree, "Event" );
	_ftw.createBranch( this->_tree, "Tracks" );
	_fmtdw.createBranch( this->_tree, "MtdPidTraits" );
	_fbtofw.createBranch( this->_tree, "BTofPidTraits" );
	this->_fhw.createBranch( this->_tree, "Helices" );


	h_event_stats = new TH1F( "event_stats", "", 100, 0, 100 );
	h_vz = new TH1F( "vz", ";TPC_vz (cm)", 500, -250, 250 );
	h_vr = new TH1F( "vr", ";TPC_vr (cm)", 500, 0, 5 );
	h_delta_vz = new TH1F( "delta_vz", ";VPD_vz - TPC_vz (cm)", 200, -50, 50 );
	h_delta_vz_w_pos_ranking = new TH1F( "delta_vz_w_pos_ranking", ";VPD_vz - TPC_vz (cm)", 200, -50, 50 );
	h_ranking = new TH1F( "ranking", ";ranking", 20, -5, 5 );

	return kStOK;
}

Int_t FemtoDstWriter::Finish()
{
	LOG_INFO << "FINISH" <<endm;

	this->_rootFile->Write();
	this->_rootFile->Close();
	return kStOK;
}

Int_t FemtoDstWriter::Make()
{
	LOG_DEBUG << "FemtoDstWriter::Make()" << endm;
	

	StPicoDstMaker * picoDstMaker = (StPicoDstMaker*) GetMaker( "picoDst" );
	if ( nullptr == picoDstMaker ){
		LOG_INFO << "No StPicoDstMaker found, exit" << endm;
		return kStWarn;
	}

	this->_StPicoDst = picoDstMaker->picoDst();
	if ( nullptr == this->_StPicoDst ){
		LOG_INFO << "No StPicoDst found, exit" << endm;
		return kStWarn;
	}

	this->_StPicoEvent = this->_StPicoDst->event();
	if ( nullptr == this->_StPicoEvent ){
		LOG_INFO << "No StPicoEvent found, exit" << endm;
		return kStWarn;
	}

	h_event_stats->Fill(0);
	if ( false == this->_StPicoEvent->isDiMuon() )
		return kStOK;
	


	this->_fmtEvent.mRunId    = this->_StPicoEvent->runId();
	this->_fmtEvent.mEventId  = this->_StPicoEvent->eventId();
	this->_fmtEvent.mGRefMult = this->_StPicoEvent->grefMult();
	this->_fmtEvent.mBin16    = this->_StPicoEvent->refMult();

	this->_fmtEvent.vertex( this->_StPicoEvent->primaryVertex().x(), this->_StPicoEvent->primaryVertex().y(), this->_StPicoEvent->primaryVertex().z() );

	float vr = sqrt( pow(this->_StPicoEvent->primaryVertex().x(), 2) + pow(this->_StPicoEvent->primaryVertex().y(), 2) );
	h_vr->Fill( vr );
	h_vz->Fill( this->_StPicoEvent->primaryVertex().z() );
	h_event_stats->Fill(1);
	
	if ( fabs(this->_StPicoEvent->primaryVertex().z()) > 100.0 )
		return kStOK;

	float deltaVz = this->_StPicoEvent->vzVpd() - this->_StPicoEvent->primaryVertex().z();
	h_delta_vz->Fill( deltaVz );
	if (this->_StPicoEvent->ranking() > 0)
		h_delta_vz_w_pos_ranking->Fill( deltaVz );

	h_event_stats->Fill(2);
	if ( fabs( deltaVz ) > 6.0  )
		return kStOK;

	h_ranking->Fill( this->_StPicoEvent->ranking() / 1e6 );
	h_event_stats->Fill(3);
	if ( this->_StPicoEvent->ranking() < 0 )
		return kStOK;

	h_event_stats->Fill(4);
	_few.set( this->_fmtEvent );

	// RESET containers
	this->_ftw.reset();
	this->_fhw.reset();
	this->_fmtdw.reset();
	this->_fbtofw.reset();

	for(unsigned int i=0;i<this->_StPicoDst->numberOfTracks();i++) {
		StPicoTrack *tr =  (StPicoTrack*) this->_StPicoDst->track(i);
		if(!tr) continue;

		this->_fmtTrack.reset();
		addMtdPidTraits( tr );
		addTrack( tr );
	}

	this->_tree->Fill();

	return kStOK;

}

void FemtoDstWriter::addTrack( StPicoTrack *muTrack )
{

	if ( nullptr == muTrack ){
		LOG_INFO << "WARN, null StPicoTrack" << endm;
		return;
	}

	this->_fmtTrack.mNHitsMax = muTrack->nHitsMax();
	this->_fmtTrack.mNHitsFit = muTrack->nHitsFit() * muTrack->charge();
	this->_fmtTrack.mNHitsDedx = muTrack->nHitsDedx();

	this->_fmtTrack.dEdx( muTrack->dEdx() );
	this->_fmtTrack.nSigmaElectron( muTrack->nSigmaElectron() );
	this->_fmtTrack.nSigmaPion( muTrack->nSigmaPion() );
	this->_fmtTrack.nSigmaKaon( muTrack->nSigmaKaon() );
	this->_fmtTrack.nSigmaProton( muTrack->nSigmaProton() );

	StThreeVectorF p = muTrack->pMom();
	this->_fmtTrack.mPt  = p.perp();
	this->_fmtTrack.mEta = p.pseudoRapidity();
	this->_fmtTrack.mPhi = p.phi();

	this->_fmtTrack.mId = muTrack->id() - 1; // id starts at 1
	
	this->_fmtTrack.gDCA( muTrack->dca() );

	this->_ftw.add( this->_fmtTrack );
}


void FemtoDstWriter::addMtdPidTraits( StPicoTrack *muTrack )
{

	StPicoMtdPidTraits *mtdPid =  (StPicoMtdPidTraits*) this->_StPicoDst->mtdPidTraits(  muTrack->mtdPidTraitsIndex() );

	this->_fmtMtdPid.reset();
	this->_fmtMtdPid.mDeltaY            = mtdPid->deltaY();
	this->_fmtMtdPid.mDeltaZ            = mtdPid->deltaZ();
	this->_fmtMtdPid.mMatchFlag         = mtdPid->matchFlag();
	this->_fmtMtdPid.mDeltaTimeOfFlight = mtdPid->deltaTimeOfFlight();
	this->_fmtMtdPid.mMtdHitChan        = (mtdPid->backleg() - 1) * 60 + (mtdPid->module() - 1) * 12 + mtdPid->cell();
	this->_fmtMtdPid.mIdTruth           = -1;
	this->_fmtMtdPid.mTriggerFlag       = -1;

	unsigned int nMtdHits = this->_StPicoDst->numberOfMtdHits();
	// look for trigger info
	if ( nMtdHits > 0 ){
		for ( int iMtdHit = 0; iMtdHit < nMtdHits; iMtdHit++ ){
			StPicoMtdHit *hit = this->_StPicoDst->mtdHit(iMtdHit);
			
			if(mtdPid->backleg()==hit->backleg() && mtdPid->module()==hit->module() && mtdPid->cell()==hit->cell()){
				this->_fmtMtdPid.mTriggerFlag = hit->triggerFlag();
				break;
			}
		} // loop iMtdHit
	} // MtdHit_ > 0

	this->_fmtTrack.mMtdPidTraitsIndex = this->_fmtdw.N();

	this->_fmtdw.add( this->_fmtMtdPid );
}


void FemtoDstWriter::addBTofPidTraits( StPicoTrack *muTrack )
{
	

	// StTrack *track = node->track( primary );
	// if ( nullptr == track ) 
	// 	return;
	// StPtrVecTrackPidTraits traits = track->pidTraits(kTofId);
	
	// if ( traits.size() <= 0 )
	// 	return;

	// StBTofPidTraits *btofPid = dynamic_cast<StBTofPidTraits*>(traits[0]);
	// if ( nullptr == btofPid ){
	// 	LOG_INFO << "WARN, null BTofPidTraits" << endm;
	// 	return;
	// }

	// if ( Debug() ) {
	// 	LOG_INFO << "MC BTofHit? " << btofPid->tofHit()->idTruth() << endm;
	//  	LOG_INFO << "MC BTofHit? QA = " << btofPid->tofHit()->qaTruth() << endm;
	
	// 	if ( btofPid->tofHit()->associatedTrack() ){
	// 		LOG_INFO << "MC BTofHit Track " << btofPid->tofHit()->associatedTrack()->key() << endm;
	// 		LOG_INFO << "this track keey = " << track->key() << endm;
	// 	}
	// }

	// StBTofHit *hit = btofPid->tofHit();

	// this->_fmtBTofPid.reset();
	// this->_fmtBTofPid.yLocal (btofPid->yLocal() );
	// this->_fmtBTofPid.zLocal( btofPid->zLocal() );
	// this->_fmtBTofPid.matchFlag( btofPid->matchFlag() );
	// this->_fmtBTofPid.mIdTruth  =  btofPid->tofHit()->idTruth() - 1; // minus 1 to index at 0
	
	// double b = btofPid->beta();
	// if ( b < 0 )
	// 	b = 0;
	// this->_fmtBTofPid.beta( b );


	// this->_fmtTrack.mBTofPidTraitsIndex = this->_fbtofw.N();

	// this->_fbtofw.add( this->_fmtBTofPid );

}
