//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "scoring.hpp"
#include <wikiedit.hpp>
//#include <wikiuser.hpp>
#include <wikisite.hpp>
#include <wikipage.hpp>
#include <configuration.hpp>
#include <querypool.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <syslog.hpp>
#include <QTimer>

#define SCORING_TIMEOUT this->GetConfig("timeout", "7")

scoring::scoring()
{
    this->tm = new QTimer();
    connect(this->tm, SIGNAL(timeout()), this, SLOT(Refresh()));
    this->tm->start(HUGGLE_TIMER);
}

scoring::~scoring()
{
    this->tm->stop();
    delete this->tm;
}

bool scoring::Register()
{
    this->Init();
    return true;
}

bool scoring::IsWorking()
{
    return true;
}

void scoring::Hook_Shutdown()
{
    this->SetConfig("server", this->GetConfig("server", this->GetServer()));
    foreach (Huggle::WikiSite *sx, hcfg->Projects)
        this->SetConfig(sx->Name + "_amp", QString::number(this->GetAmplifier(sx)));
    this->SetConfig("timeout", SCORING_TIMEOUT);
}

void scoring::Hook_MainWindowOnLoad(void *window)
{
    foreach (Huggle::WikiSite *x, hcfg->Projects)
    {
        if (x->Name != "enwiki")
        {
            Huggle::Syslog::HuggleLogs->WarningLog(x->Name + " is not supported by scoring extension, unexpected behaviour may happen");
        }
    }
}

bool scoring::Hook_EditIsReady(void *edit)
{
    if (this->Edits.contains(edit))
        return false;

    return true;
}

bool scoring::Hook_EditBeforeScore(void *edit)
{
    return true;
}

void scoring::Hook_EditBeforePostProcessing(void *edit)
{
    if (this->Edits.contains(edit))
    {
        HUGGLE_EXDEBUG1("edit " + QString::number((intptr_t) edit, 16) + " is already in list");
        return;
    }
    Huggle::WikiEdit *WikiEdit = (Huggle::WikiEdit*)edit;
    if (WikiEdit->RevID == WIKI_UNKNOWN_REVID)
        return;
    WikiEdit->IncRef();
    Huggle::Collectable_SmartPtr<Huggle::WebserverQuery> query = new Huggle::WebserverQuery();
    query->Timeout = SCORING_TIMEOUT.toInt();
    query->URL = this->GetServer() + WikiEdit->GetSite()->Name + "/reverted/" + QString::number(WikiEdit->RevID) + "/";
    query->Process();
    Huggle::QueryPool::HugglePool->AppendQuery(query);
    this->Edits.insert(edit, query);
    return;
}

void scoring::Hook_GoodEdit(void *edit)
{

}

double scoring::GetAmplifier(Huggle::WikiSite *site)
{
    return this->GetConfig(site->Name + "_amp", "200").toDouble();
}

void scoring::Refresh()
{
    foreach (void* edit, this->Edits.keys())
    {
        Huggle::Collectable_SmartPtr<Huggle::WebserverQuery> request = this->Edits[edit];
        Huggle::Collectable_SmartPtr<Huggle::WikiEdit> wiki_edit = (Huggle::WikiEdit*)edit;
        if (request->IsProcessed())
        {
            this->Edits.remove(edit);
            wiki_edit->DecRef();
            if (request->IsFailed())
            {
                Huggle::Syslog::HuggleLogs->ErrorLog("Scoring failed for edit " + wiki_edit->Page->PageName + ": " + request->GetFailureReason());
            } else
            {
                //HUGGLE_EXDEBUG(request->Result->Data, 3);
                QJsonDocument d = QJsonDocument::fromJson(request->Result->Data.toUtf8());
                QJsonObject score = d.object();
                QString revid = QString::number(wiki_edit->RevID);
                if (!score.contains(revid))
                {
                    HUGGLE_EXDEBUG1("Revision score for edit " + revid + " did not contain the information for this revision, source code was: " + request->Result->Data);
                    continue;
                }
                QJsonObject rev = score[revid].toObject();
                if (!rev.contains("prediction"))
                {
                    HUGGLE_EXDEBUG1("Revision didn't contain prediction for " + revid);
                    continue;
                }
                if (!rev.contains("probability"))
                {
                    HUGGLE_EXDEBUG1("Revision didn't contain probability for " + revid);
                    continue;
                }
                // We don't need this for anything TBH
                //bool prediction = rev["prediction"].toBool();
                QJsonObject probability = rev["probability"].toObject();
                if (!probability.contains("false") || !probability.contains("true"))
                {
                    HUGGLE_EXDEBUG1("Probability information was invalid for " + revid);
                    continue;
                }
                // Math
                long final = (long)((probability["true"].toDouble() - probability["false"].toDouble()) * this->GetAmplifier(wiki_edit->GetSite()));
                wiki_edit->Score += final;
                if (!wiki_edit->MetaLabels.contains("ORES Score"))
                {
                    // Insert meta
                    wiki_edit->MetaLabels.insert("ORES Score", QString::number(final));
                }
            }
        }
    }
}

QString scoring::GetServer()
{
    return this->GetConfig("server",  "http://ores.wmflabs.org/scores/");
}

#if QT_VERSION < 0x050000
    Q_EXPORT_PLUGIN2("org.huggle.extension.qt", scoring)
#endif

